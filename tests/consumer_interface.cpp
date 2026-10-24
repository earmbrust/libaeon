/*********************************************************************
 * Aeon 2.0 public consumer smoke test.
 * Built as distinct executables against shared and static Aeon targets.
 * Exercises actual library calls and loopback datagrams; no mocks.
 *********************************************************************/

#include <aeon.hpp>
#include <array>
#include <cstddef>
#include <concepts>
#include <span>
#include <string_view>
#include <type_traits>
#include <cstring>
#include <iostream>
#include <string>

#if defined(_MSVC_LANG)
static_assert(_MSVC_LANG >= 202002L, "Aeon requires C++20");
#else
static_assert(__cplusplus >= 202002L, "Aeon requires C++20");
#endif

static_assert(std::derived_from<aeon::client_socket, aeon::socket>);
static_assert(std::derived_from<aeon::server_socket, aeon::socket>);
#if LIBAEON_HAS_EVENTS
static_assert(std::derived_from<aeon::event_socket, aeon::socket>);
#endif
static_assert(std::derived_from<aeon::udp_socket, aeon::socket>);
#ifdef ENABLE_SSL
static_assert(std::derived_from<aeon::ssl_client_socket, aeon::socket>);
#endif

namespace {

int fail(const char* message) {
    std::cerr << "Aeon consumer failure: " << message << '\n';
    return 1;
}

// Consumer-defined subclasses must be able to call protected, exported
// event-dispatch methods in Aeon, with callbacks registered by consumers.
class consumer_hostname_probe final : public aeon::socket {
public:
    using aeon::socket::remote_hostname;
    using aeon::socket::set_remote_hostname;
};

#if LIBAEON_HAS_EVENTS
class client_event_probe final : public aeon::event_socket {
public:
    void emit_data(const char* data, int count) { fire_on_data(data, count); }
    void emit_drain() { fire_on_drain(); }
};

class client_event_set_probe final : public aeon::event_socket_set {
public:
    void emit_data(aeon::event_socket* source, const char* data, int count) {
        fire_on_socket_data(source, data, count);
    }
    void emit_drain(aeon::event_socket* source) { fire_on_socket_drain(source); }
};


#endif

} // namespace

int main() {
    // The exported version symbol must resolve for both linking variants.
    std::array<std::byte, 4> bytes{};
    std::span<std::byte> writable(bytes);
    writable[0] = std::byte{0x42};
    const std::span<const std::byte> readable = writable;
    if (readable.size() != 4 || readable[0] != std::byte{0x42}) {
        return fail("C++20 span");
    }

    const char* version = aeon::get_library_version();
    if (!version || !*version) return fail("exported library version");

    aeon::socket base;
    if (!base.is_valid_socket()) return fail("native socket construction");

    // Exercise opaque hostname state through exported members of a consumer-
    // defined subclass, on both shared and static library boundaries.
    consumer_hostname_probe host;
    if (!host.remote_hostname().empty()) return fail("empty hostname state");
    host.set_remote_hostname("localhost");
    if (host.remote_hostname() != "localhost") return fail("hostname state");

    // socket_set's out-of-line constructor, methods and destructor all live
    // on the correct side of the shared-library boundary.
    {
        aeon::socket_set sockets;
        if (!sockets.add(&base) || sockets.size() != 1) {
            return fail("socket_set::add/size");
        }
        if (!sockets.remove(0) || sockets.size() != 0) {
            return fail("socket_set::remove/size");
        }
        if (!sockets.add(&base) || !sockets.add(&host) ||
            !sockets.remove(0, 999) || sockets.size() != 0 ||
            !base.is_valid_socket() || !host.is_valid_socket()) {
            return fail("socket_set opaque bulk removal");
        }
    }

    #if LIBAEON_HAS_EVENTS
    client_event_probe event_socket;
    bool local_callback_ran = false;
    event_socket.on_data([&](const char* data, int count) {
        local_callback_ran = count == 4 && std::memcmp(data, "ping", 4) == 0;
    });
    event_socket.emit_data("ping", 4);
    if (!local_callback_ran) return fail("event_socket consumer callback");
    bool local_drain = false;
    event_socket.on_drain([&] { local_drain = true; });
    event_socket.emit_drain();
    if (!local_drain) return fail("event_socket drain callback");
    if (event_socket.queue_write("x", 1)) return fail("unconnected queue rejected");
    if (event_socket.queued_bytes() != 0 ||
        !event_socket.set_max_queued_bytes(4096)) return fail("queue consumer API");

    // A C++20 consumer registers a callback invoked by the library.
    // This checks both DLL linkage and actual callback invocation.
    {
        client_event_set_probe events;
        bool set_callback_ran = false;
        events.on_socket_data([&](aeon::event_socket* source,
                                    const char* data, int count) {
            set_callback_ran = source == &event_socket && count == 4 &&
                               std::memcmp(data, "pong", 4) == 0;
        });
        if (events.poll(0) != 0) return fail("empty event poll");
        if (!events.add(&event_socket) || events.size() != 1) {
            return fail("event_socket_set::add/size");
        }
        events.emit_data(&event_socket, "pong", 4);
        if (!set_callback_ran) return fail("event_socket_set consumer callback");
        bool set_drain = false;
        events.on_socket_drain([&](aeon::event_socket* source) {
            set_drain = source == &event_socket;
        });
        events.emit_drain(&event_socket);
        if (!set_drain) return fail("event_socket_set drain callback");
        if (!events.remove(0) || events.size() != 0) {
            return fail("event_socket_set::remove/size");
        }
    }

    #endif

    // Real UDP loopback round trip with an OS-selected ephemeral port.
    // Prefer the server's actual address family so IPv6-only and IPv4
    // environments can both exercise sending and receiving.
    aeon::udp_server_socket server;
    if (!server.listen(0)) return fail("UDP bind to ephemeral port");
    sockaddr_storage local_address{};
    socklen_t local_length = static_cast<socklen_t>(sizeof(local_address));
    if (::getsockname(server.sockfd,
                      reinterpret_cast<sockaddr*>(&local_address),
                      &local_length) != 0) {
        return fail("UDP getsockname");
    }
    const char* loopback = nullptr;
    unsigned short port = 0;
    if (local_address.ss_family == AF_INET6) {
        loopback = "::1";
        port = ntohs(reinterpret_cast<const sockaddr_in6*>(&local_address)->sin6_port);
    } else if (local_address.ss_family == AF_INET) {
        loopback = "127.0.0.1";
        port = ntohs(reinterpret_cast<const sockaddr_in*>(&local_address)->sin_port);
    } else {
        return fail("UDP unsupported address family");
    }
    if (port == 0) return fail("UDP ephemeral port assignment");

    aeon::udp_client_socket sender;
    if (!sender.connect(loopback, static_cast<int>(port))) {
        return fail("UDP configure sender");
    }
    if (server.set_read_timeout(2000) != 0 ||
        sender.set_read_timeout(2000) != 0) {
        return fail("UDP timeout configuration");
    }
    if (sender.write("PING", 4) != 4) return fail("UDP send from client");
    char buffer[32]{};
    const int count = server.read(buffer, static_cast<int>(sizeof(buffer)));
    if (count != 4 || std::memcmp(buffer, "PING", 4) != 0) {
        return fail("UDP server receive");
    }
    if (server.write("PONG", 4) != 4) return fail("UDP server reply");
    std::memset(buffer, 0, sizeof(buffer));
    const int reply = sender.read(buffer, static_cast<int>(sizeof(buffer)));
    if (reply != 4 || std::memcmp(buffer, "PONG", 4) != 0) {
        return fail("UDP client receive");
    }

#ifdef ENABLE_SSL
    // Tests public SSL symbol availability, constructor/destructor and a real
    // SSL_CTX-backed configuration operation. Handshake tests remain separate.
    aeon::ssl_client_socket tls_client;
    if (tls_client.set_peer_verification(aeon::ssl_verify_mode::strict) !=
        aeon::err_none) {
        return fail("SSL configuration via public API");
    }
#endif

    std::cout << "Aeon consumer interface PASSED (" << version << ")\n";
    return 0;
}
