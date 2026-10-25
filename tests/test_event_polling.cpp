#include <gtest/gtest.h>
#include "aeon.hpp"

#include <chrono>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace {
int port_of(const aeon::socket& owner) {
    sockaddr_storage bound{};
    socklen_t len = static_cast<socklen_t>(sizeof(bound));
    if (::getsockname(owner.sockfd, reinterpret_cast<sockaddr*>(&bound), &len) != 0)
        return -1;
    if (bound.ss_family == AF_INET)
        return ntohs(reinterpret_cast<const sockaddr_in*>(&bound)->sin_port);
    if (bound.ss_family == AF_INET6)
        return ntohs(reinterpret_cast<const sockaddr_in6*>(&bound)->sin6_port);
    return -1;
}

const char* loopback_of(const aeon::socket& owner) {
    sockaddr_storage bound{};
    socklen_t len = static_cast<socklen_t>(sizeof(bound));
    if (::getsockname(owner.sockfd, reinterpret_cast<sockaddr*>(&bound), &len) != 0)
        return nullptr;
    if (bound.ss_family == AF_INET) return "127.0.0.1";
    if (bound.ss_family == AF_INET6) return "::1";
    return nullptr;
}

// Test-only readiness probe: ensure both TCP streams have pending bytes
// before asserting cancellation behavior from one poll snapshot.
bool wait_until_readable(aeon::socket_t fd) {
    fd_set readable;
    FD_ZERO(&readable);
    FD_SET(fd, &readable);
    timeval timeout{2, 0};
#ifdef NET_PLATFORM_WINDOWS
    constexpr int nfds = 0; // Winsock ignores nfds.
#else
    const int nfds = static_cast<int>(fd) + 1;
#endif
    return ::select(nfds, &readable, nullptr, nullptr, &timeout) == 1 &&
        FD_ISSET(fd, &readable);
}

struct tcp_fixture {
    aeon::server_event_socket listener;
    aeon::client_socket sender;
    aeon::event_socket* receiver = nullptr; // listener owns it

    bool start() {
        if (!listener.listen("127.0.0.1", 0)) return false;
        const int port = port_of(listener);
        if (port <= 0 || !sender.connect("127.0.0.1", port)) return false;
        receiver = listener.accept(false);
        return static_cast<bool>(receiver);
    }
};
} // namespace

TEST(EventPolling, RejectsInvalidTimeoutAndEmptySetDoesNotBlockIndefinitely) {
    aeon::event_socket_set set;
    EXPECT_EQ(set.poll(-2), -1);
    EXPECT_EQ(set.poll(0), 0);
    EXPECT_EQ(set.poll(-1), 0); // No sockets: never wait forever.
    const auto before = std::chrono::steady_clock::now();
    EXPECT_EQ(set.poll(25), 0);
    const auto elapsed = std::chrono::steady_clock::now() - before;
    EXPECT_GE(elapsed, std::chrono::milliseconds(15));
}

TEST(EventPolling, TcpListeningReadinessAcceptsAndRegistersWithoutThreads) {
    aeon::server_event_socket listener;
    ASSERT_TRUE(listener.listen("127.0.0.1", 0));
    aeon::event_socket_set set;
    ASSERT_TRUE(set.add(&listener));
    aeon::event_socket* peer = nullptr; // listener owns it
    int listen_hits = 0;
    set.on_socket_listen([&](aeon::event_socket* ready) {
        EXPECT_EQ(ready, &listener);
        ++listen_hits;
        peer = listener.accept(false);
        ASSERT_NE(peer, nullptr);
        EXPECT_TRUE(set.add(peer));
    });
    aeon::client_socket client;
    ASSERT_TRUE(client.connect("127.0.0.1", port_of(listener)));
    ASSERT_GT(set.poll(2000), 0);
    EXPECT_EQ(listen_hits, 1);
    ASSERT_NE(peer, nullptr);
    int receive_hits = 0;
    peer->on_data([&](const char* data, int bytes) {
        ++receive_hits;
        EXPECT_EQ(std::string(data, static_cast<size_t>(bytes)), "ping");
    });
    ASSERT_EQ(client.write("ping", 4), 4);
    EXPECT_GT(set.poll(2000), 0);
    EXPECT_EQ(receive_hits, 1);
    EXPECT_TRUE(set.remove_socket(peer));
    EXPECT_TRUE(set.remove_socket(&listener));
}

TEST(EventPolling, SocketAndSetHandlersBothRunWithBinaryData) {
    tcp_fixture pair;
    ASSERT_TRUE(pair.start());
    aeon::event_socket_set set;
    ASSERT_TRUE(set.add(pair.receiver));
    const char data[5] = {'a', '\0', 'b', 'c', 'd'};
    int local_hits = 0;
    pair.receiver->on_data([&](const char* b, int n) {
        ++local_hits;
        ASSERT_EQ(n, 5);
        EXPECT_EQ(std::memcmp(b, data, sizeof(data)), 0);
    });
    int set_hits = 0;
    set.on_socket_data([&](aeon::event_socket* s, const char* b, int n) {
        ++set_hits;
        EXPECT_EQ(s, pair.receiver);
        ASSERT_EQ(n, 5);
        EXPECT_EQ(std::memcmp(b, data, sizeof(data)), 0);
    });
    ASSERT_EQ(pair.sender.write(data, 5), 5);
    EXPECT_GT(set.poll(2000), 0);
    EXPECT_EQ(local_hits, 1);
    EXPECT_EQ(set_hits, 1);
    EXPECT_EQ(set.poll(0), 0);
    EXPECT_TRUE(set.remove_socket(pair.receiver));
}

TEST(EventPolling, CallbackCanRemoveAndReaddItsOwnRegistration) {
    tcp_fixture pair;
    ASSERT_TRUE(pair.start());
    aeon::event_socket_set set;
    ASSERT_TRUE(set.add(pair.receiver));
    int count = 0;
    set.on_socket_data([&](aeon::event_socket* s, const char*, int n) {
        count += n;
        EXPECT_TRUE(set.remove_socket(s));
    });
    ASSERT_EQ(pair.sender.write("one", 3), 3);
    EXPECT_GT(set.poll(2000), 0);
    EXPECT_EQ(count, 3);
    EXPECT_EQ(set.size(), 0);
    EXPECT_TRUE(pair.receiver->is_valid_socket());
    EXPECT_TRUE(set.add(pair.receiver));
    ASSERT_EQ(pair.sender.write("two", 3), 3);
    EXPECT_GT(set.poll(2000), 0);
    EXPECT_EQ(count, 6);
}

TEST(EventPolling, SocketCallbackRemovalSkipsSetFanout) {
    tcp_fixture pair;
    ASSERT_TRUE(pair.start());
    aeon::event_socket_set set;
    ASSERT_TRUE(set.add(pair.receiver));
    int local = 0, global = 0;
    pair.receiver->on_data([&](const char*, int) {
        ++local;
        EXPECT_TRUE(set.remove_socket(pair.receiver));
    });
    set.on_socket_data([&](aeon::event_socket*, const char*, int) { ++global; });
    ASSERT_EQ(pair.sender.write("a", 1), 1);
    EXPECT_GT(set.poll(2000), 0);
    EXPECT_EQ(local, 1);
    EXPECT_EQ(global, 0);
}

TEST(EventPolling, PeerEofClosesReceiverAndNotifiesSet) {
    tcp_fixture pair;
    ASSERT_TRUE(pair.start());
    aeon::event_socket_set set;
    ASSERT_TRUE(set.add(pair.receiver));
    int socket_closes = 0, set_closes = 0;
    pair.receiver->on_close([&] { ++socket_closes; });
    set.on_socket_close([&](aeon::event_socket* s) {
        ++set_closes;
        EXPECT_EQ(s, pair.receiver);
        EXPECT_TRUE(set.remove_socket(s));
    });
    ASSERT_TRUE(pair.sender.close());
    EXPECT_GT(set.poll(2000), 0);
    EXPECT_EQ(socket_closes, 1);
    EXPECT_EQ(set_closes, 1);
    EXPECT_FALSE(pair.receiver->is_valid_socket());
    EXPECT_EQ(set.size(), 0);
    EXPECT_EQ(set.poll(0), 0);
}

TEST(EventPolling, ReentrantPollReturnsErrorRatherThanDeadlocking) {
    tcp_fixture pair;
    ASSERT_TRUE(pair.start());
    aeon::event_socket_set set;
    ASSERT_TRUE(set.add(pair.receiver));
    int nested = 0;
    set.on_socket_data([&](aeon::event_socket*, const char*, int) { nested = set.poll(0); });
    ASSERT_EQ(pair.sender.write("x", 1), 1);
    EXPECT_GT(set.poll(2000), 0);
    EXPECT_EQ(nested, -1);
}

TEST(EventPolling, UdpSenderAddressAndZeroLengthDatagramsAreDelivered) {
    aeon::udp_server_event_socket listener;
    ASSERT_TRUE(listener.listen(0));
    const char* host = loopback_of(listener);
    ASSERT_NE(host, nullptr);
    aeon::udp_client_socket sender;
    ASSERT_TRUE(sender.connect(host, port_of(listener)));
    aeon::event_socket_set set;
    ASSERT_TRUE(set.add(&listener));
    int local = 0, global = 0, last_len = -1;
    listener.on_datagram([&](const char* d, int n, const aeon::address& from) {
        ++local;
        last_len = n;
        EXPECT_GT(from.get_port(), 0);
        if (n) EXPECT_EQ(std::string(d, static_cast<size_t>(n)), "data");
    });
    set.on_socket_datagram([&](aeon::udp_event_socket* s, const char*, int n, const aeon::address& from) {
        ++global;
        EXPECT_EQ(s, &listener);
        EXPECT_EQ(n, last_len);
        EXPECT_GT(from.get_port(), 0);
    });
    ASSERT_EQ(sender.write("data", 4), 4);
    EXPECT_GT(set.poll(2000), 0);
    EXPECT_EQ(local, 1);
    EXPECT_EQ(global, 1);
    EXPECT_EQ(last_len, 4);
    const auto destination = sender.remote_addr;
    const auto size = destination.ss_family == AF_INET6
        ? static_cast<socklen_t>(sizeof(sockaddr_in6))
        : static_cast<socklen_t>(sizeof(sockaddr_in));
    ASSERT_EQ(::sendto(sender.sockfd, "", 0, 0,
        reinterpret_cast<const sockaddr*>(&destination), size), 0);
    EXPECT_GT(set.poll(2000), 0);
    EXPECT_EQ(local, 2);
    EXPECT_EQ(global, 2);
    EXPECT_EQ(last_len, 0);
    EXPECT_TRUE(set.remove_udp_socket(&listener));
}

TEST(EventPolling, UdpCallbackCanDeregisterBeforeSetFanout) {
    aeon::udp_server_event_socket listener;
    ASSERT_TRUE(listener.listen(0));
    aeon::udp_client_socket sender;
    const char* host = loopback_of(listener);
    ASSERT_NE(host, nullptr);
    ASSERT_TRUE(sender.connect(host, port_of(listener)));
    aeon::event_socket_set set;
    ASSERT_TRUE(set.add(&listener));
    int local = 0, global = 0;
    listener.on_datagram([&](const char*, int, const aeon::address&) {
        ++local;
        EXPECT_TRUE(set.remove_udp_socket(&listener));
    });
    set.on_socket_datagram([&](aeon::udp_event_socket*, const char*, int, const aeon::address&) { ++global; });
    ASSERT_EQ(sender.write("z", 1), 1);
    EXPECT_GT(set.poll(2000), 0);
    EXPECT_EQ(local, 1);
    EXPECT_EQ(global, 0);
    EXPECT_EQ(set.size(), 0);
    EXPECT_TRUE(listener.is_valid_socket());
}

TEST(EventPolling, MixedRegistrationAndIndexRemovalRemainNonOwning) {
    aeon::event_socket tcp;
    aeon::udp_event_socket udp;
    const auto fd = udp.sockfd;
    aeon::event_socket_set set;
    EXPECT_FALSE(set.add(nullptr));
    EXPECT_TRUE(set.add(&udp));
    EXPECT_FALSE(set.add(&udp));
    EXPECT_TRUE(set.add(&tcp));
    EXPECT_EQ(set.size(), 2);
    EXPECT_TRUE(set.remove(0U)); // TCP first.
    EXPECT_TRUE(set.remove(0U)); // Then UDP.
    EXPECT_EQ(set.size(), 0);
    EXPECT_TRUE(udp.is_valid_socket());
    EXPECT_EQ(udp.sockfd, fd);
    EXPECT_TRUE(tcp.is_valid_socket());
}

TEST(EventPolling, ClosedRegisteredSocketsAreSkipped) {
    aeon::event_socket tcp;
    aeon::udp_event_socket udp;
    aeon::event_socket_set set;
    ASSERT_TRUE(set.add(&tcp));
    ASSERT_TRUE(set.add(&udp));
    ASSERT_TRUE(tcp.close());
    ASSERT_TRUE(udp.close());
    EXPECT_EQ(set.poll(0), 0);
    EXPECT_EQ(set.size(), 2);
    EXPECT_TRUE(set.remove_socket(&tcp));
    EXPECT_TRUE(set.remove_udp_socket(&udp));
}

TEST(EventPolling, MoreThanOneHundredTcpClientsAndUdpShareOnePoller) {
    aeon::server_event_socket server;
    ASSERT_TRUE(server.listen("127.0.0.1", 0));
    const int port = port_of(server);
    ASSERT_GT(port, 0);
    aeon::udp_server_event_socket udp;
    ASSERT_TRUE(udp.listen(0));
    const char* host = loopback_of(udp);
    ASSERT_NE(host, nullptr);
    aeon::udp_client_socket udp_sender;
    ASSERT_TRUE(udp_sender.connect(host, port_of(udp)));

    aeon::event_socket_set set;
    std::vector<std::unique_ptr<aeon::client_socket>> senders;
    std::vector<aeon::event_socket*> receivers; // server owns these
    constexpr int total = 105; // Above Windows select() FD_SETSIZE default.
    int tcp_bytes = 0, udp_datagrams = 0;
    set.on_socket_data([&](aeon::event_socket*, const char*, int n) { tcp_bytes += n; });
    set.on_socket_datagram([&](aeon::udp_event_socket* s, const char*, int n, const aeon::address&) {
        EXPECT_EQ(s, &udp);
        EXPECT_EQ(n, 2);
        ++udp_datagrams;
    });
    for (int i = 0; i < total; ++i) {
        auto client = std::make_unique<aeon::client_socket>();
        ASSERT_TRUE(client->connect("127.0.0.1", port));
        auto peer = server.accept(false);
        ASSERT_NE(peer, nullptr);
        ASSERT_TRUE(set.add(peer));
        ASSERT_EQ(client->write("!", 1), 1);
        senders.push_back(std::move(client));
        receivers.push_back(peer);
    }
    ASSERT_TRUE(set.add(&udp));
    ASSERT_EQ(udp_sender.write("ok", 2), 2);
    for (int attempt = 0; attempt != 10 && (tcp_bytes < total || udp_datagrams < 1); ++attempt)
        ASSERT_GE(set.poll(1500), 0);
    EXPECT_EQ(tcp_bytes, total);
    EXPECT_EQ(udp_datagrams, 1);
    while (set.size() != 0) EXPECT_TRUE(set.remove(0U));
}

TEST(EventPolling, ExceptionInCallbackReleasesSinglePollGuard) {
    tcp_fixture pair;
    ASSERT_TRUE(pair.start());
    aeon::event_socket_set set;
    ASSERT_TRUE(set.add(pair.receiver));
    int calls = 0;
    set.on_socket_data([&](aeon::event_socket*, const char*, int) {
        ++calls;
        throw std::runtime_error("deliberate callback exception");
    });
    ASSERT_EQ(pair.sender.write("a", 1), 1);
    bool caught = false;
    try { set.poll(2000); }
    catch (const std::runtime_error&) { caught = true; }
    EXPECT_TRUE(caught);
    EXPECT_EQ(calls, 1);
    set.on_socket_data([&](aeon::event_socket*, const char*, int) { ++calls; });
    ASSERT_EQ(pair.sender.write("b", 1), 1);
    EXPECT_GT(set.poll(2000), 0);
    EXPECT_EQ(calls, 2);
    EXPECT_TRUE(set.remove_socket(pair.receiver));
}

TEST(EventPolling, ListenerCallbackCanDeregisterAndCloseListener) {
    aeon::server_event_socket listener;
    ASSERT_TRUE(listener.listen("127.0.0.1", 0));
    aeon::event_socket_set set;
    ASSERT_TRUE(set.add(&listener));
    int events = 0;
    set.on_socket_listen([&](aeon::event_socket* ready) {
        ++events;
        EXPECT_EQ(ready, &listener);
        EXPECT_TRUE(set.remove_socket(&listener));
        EXPECT_TRUE(listener.close());
    });
    aeon::client_socket client;
    ASSERT_TRUE(client.connect("127.0.0.1", port_of(listener)));
    EXPECT_GT(set.poll(2000), 0);
    EXPECT_EQ(events, 1);
    EXPECT_EQ(set.size(), 0);
    EXPECT_EQ(set.poll(0), 0);
}

TEST(EventPolling, UdpCallbackCanDeregisterAndCloseItself) {
    aeon::udp_server_event_socket listener;
    ASSERT_TRUE(listener.listen(0));
    const char* host = loopback_of(listener);
    ASSERT_NE(host, nullptr);
    aeon::udp_client_socket sender;
    ASSERT_TRUE(sender.connect(host, port_of(listener)));
    aeon::event_socket_set set;
    ASSERT_TRUE(set.add(&listener));
    int local = 0, global = 0;
    listener.on_datagram([&](const char*, int, const aeon::address&) {
        ++local;
        EXPECT_TRUE(set.remove_udp_socket(&listener));
        EXPECT_TRUE(listener.close());
    });
    set.on_socket_datagram([&](aeon::udp_event_socket*, const char*, int, const aeon::address&) { ++global; });
    ASSERT_EQ(sender.write("k", 1), 1);
    EXPECT_GT(set.poll(2000), 0);
    EXPECT_EQ(local, 1);
    EXPECT_EQ(global, 0);
    EXPECT_EQ(set.size(), 0);
    EXPECT_EQ(set.poll(0), 0);
}

TEST(EventPolling, NewlyRegisteredReadySocketWaitsUntilNextPoll) {
    tcp_fixture a;
    tcp_fixture b;
    ASSERT_TRUE(a.start());
    ASSERT_TRUE(b.start());
    aeon::event_socket_set set;
    ASSERT_TRUE(set.add(a.receiver));
    int first = 0, second = 0;
    set.on_socket_data([&](aeon::event_socket* socket, const char*, int) {
        if (socket == a.receiver) {
            ++first;
            EXPECT_TRUE(set.add(b.receiver));
        } else if (socket == b.receiver) {
            ++second;
        }
    });
    ASSERT_EQ(a.sender.write("a", 1), 1);
    ASSERT_EQ(b.sender.write("b", 1), 1);
    ASSERT_TRUE(wait_until_readable(a.receiver->sockfd));
    ASSERT_TRUE(wait_until_readable(b.receiver->sockfd));
    EXPECT_EQ(set.poll(2000), 1);
    EXPECT_EQ(first, 1);
    EXPECT_EQ(second, 0);
    EXPECT_EQ(set.poll(2000), 1);
    EXPECT_EQ(second, 1);
    EXPECT_TRUE(set.remove_socket(a.receiver));
    EXPECT_TRUE(set.remove_socket(b.receiver));
}

TEST(EventPolling, RemovingAnotherReadyRegistrationCancelsPendingDispatch) {
    tcp_fixture a;
    tcp_fixture b;
    ASSERT_TRUE(a.start());
    ASSERT_TRUE(b.start());
    aeon::event_socket_set set;
    ASSERT_TRUE(set.add(a.receiver));
    ASSERT_TRUE(set.add(b.receiver));
    int first = 0, second = 0;
    set.on_socket_data([&](aeon::event_socket* socket, const char*, int) {
        if (socket == a.receiver) {
            ++first;
            EXPECT_TRUE(set.remove_socket(b.receiver));
        } else if (socket == b.receiver) {
            ++second;
        }
    });
    ASSERT_EQ(a.sender.write("a", 1), 1);
    ASSERT_EQ(b.sender.write("b", 1), 1);
    ASSERT_TRUE(wait_until_readable(a.receiver->sockfd));
    ASSERT_TRUE(wait_until_readable(b.receiver->sockfd));
    EXPECT_EQ(set.poll(2000), 1);
    EXPECT_EQ(first, 1);
    EXPECT_EQ(second, 0);
    EXPECT_TRUE(b.receiver->is_valid_socket());
    EXPECT_TRUE(set.remove_socket(a.receiver));
}

TEST(EventPolling, UdpReadinessIsDeliveredOnlyToTheReadySocket) {
    aeon::udp_server_event_socket a, b;
    ASSERT_TRUE(a.listen(0));
    ASSERT_TRUE(b.listen(0));
    aeon::event_socket_set events;
    ASSERT_TRUE(events.add(&a));
    ASSERT_TRUE(events.add(&b));
    int a_hits = 0;
    int b_hits = 0;
    events.on_socket_datagram([&](aeon::udp_event_socket* source, const char*, int, const aeon::address&) {
        if (source == &a) ++a_hits;
        if (source == &b) ++b_hits;
    });
    aeon::udp_client_socket sender;
    ASSERT_TRUE(sender.connect(loopback_of(b), port_of(b)));
    ASSERT_EQ(sender.write("ready", 5), 5);
    EXPECT_GT(events.poll(2000), 0);
    EXPECT_EQ(a_hits, 0);
    EXPECT_EQ(b_hits, 1);
    EXPECT_EQ(events.poll(0), 0);
    EXPECT_TRUE(events.remove_udp_socket(&a));
    EXPECT_TRUE(events.remove_udp_socket(&b));
}

TEST(EventPolling, UdpReadinessDoesNotPersistAfterDatagramIsRead) {
    aeon::udp_server_event_socket listener;
    ASSERT_TRUE(listener.listen(0));
    aeon::event_socket_set events;
    ASSERT_TRUE(events.add(&listener));
    int received = 0;
    events.on_socket_datagram([&](aeon::udp_event_socket*, const char*, int size, const aeon::address&) {
        EXPECT_EQ(size, 1);
        ++received;
    });
    EXPECT_EQ(events.poll(0), 0);
    aeon::udp_client_socket sender;
    ASSERT_TRUE(sender.connect(loopback_of(listener), port_of(listener)));
    ASSERT_EQ(sender.write("x", 1), 1);
    EXPECT_GT(events.poll(2000), 0);
    EXPECT_EQ(received, 1);
    EXPECT_EQ(events.poll(0), 0);
    EXPECT_EQ(received, 1);
    EXPECT_TRUE(events.remove_udp_socket(&listener));
}

TEST(EventPolling, RemovingAndReaddingRegistrationDuringCallbackSkipsStaleSnapshot) {
    tcp_fixture a;
    tcp_fixture b;
    ASSERT_TRUE(a.start());
    ASSERT_TRUE(b.start());
    aeon::event_socket_set set;
    ASSERT_TRUE(set.add(a.receiver));
    ASSERT_TRUE(set.add(b.receiver));
    int first = 0;
    int second = 0;
    set.on_socket_data([&](aeon::event_socket* socket, const char*, int) {
        if (socket == a.receiver) {
            ++first;
            EXPECT_TRUE(set.remove_socket(b.receiver));
            EXPECT_TRUE(set.add(b.receiver));
        } else {
            ++second;
        }
    });
    ASSERT_EQ(a.sender.write("a", 1), 1);
    ASSERT_EQ(b.sender.write("b", 1), 1);
    ASSERT_TRUE(wait_until_readable(a.receiver->sockfd));
    ASSERT_TRUE(wait_until_readable(b.receiver->sockfd));
    EXPECT_EQ(set.poll(2000), 1);
    EXPECT_EQ(first, 1);
    EXPECT_EQ(second, 0);
    EXPECT_EQ(set.poll(2000), 1);
    EXPECT_EQ(second, 1);
    EXPECT_TRUE(set.remove_socket(a.receiver));
    EXPECT_TRUE(set.remove_socket(b.receiver));
}

// A subclass must intercept both directions of I/O, including every direct
// write overload and the queued-write path. No TLS transport may fall through
// to native recv/send when routed through event_socket_set.
namespace {
class event_transport_probe final : public aeon::event_socket {
public:
    explicit event_transport_probe(aeon::socket_t fd) : event_socket(fd, true) {}
    int receives = 0;
    int sends = 0;
protected:
    int poll_receive(char* data, int size) override {
        ++receives;
        return event_socket::poll_receive(data, size);
    }
    int poll_send(const char* data, int size, int flags) override {
        ++sends;
        return event_socket::poll_send(data, size, flags);
    }
};
} // namespace

TEST(EventPolling, TransportHooksCoverReadsDirectWritesAndWriteQueue) {
    aeon::server_event_socket listener;
    ASSERT_TRUE(listener.listen("127.0.0.1", 0));
    aeon::client_socket client;
    ASSERT_TRUE(client.connect("127.0.0.1", port_of(listener)));

    // Own the accepted descriptor in our derived event socket exactly once.
    sockaddr_storage remote{};
    socklen_t length = static_cast<socklen_t>(sizeof(remote));
    const aeon::socket_t accepted = ::accept(listener.sockfd,
        reinterpret_cast<sockaddr*>(&remote), &length);
    ASSERT_NE(accepted, aeon::invalid_socket);
    event_transport_probe peer(accepted);
    ASSERT_TRUE(peer.connected);
    aeon::event_socket_set poller;
    ASSERT_TRUE(poller.add(&peer));
    int received = 0;
    poller.on_socket_data([&](aeon::event_socket* who, const char* data, int size) {
        EXPECT_EQ(who, &peer);
        EXPECT_EQ(std::string(data, static_cast<std::size_t>(size)), "ping");
        received += size;
    });
    ASSERT_EQ(client.write("ping", 4), 4);
    ASSERT_GT(poller.poll(2000), 0);
    EXPECT_EQ(received, 4);
    EXPECT_EQ(peer.receives, 1);

    // Test the const sized overload and the queued transport, in order.
    ASSERT_EQ(peer.write("A", 1), 1);
    ASSERT_TRUE(peer.queue_write("B", 1));
    ASSERT_GT(poller.poll(2000), 0);
    char output[2]{};
    ASSERT_EQ(client.set_read_timeout(2000), 0);
    ASSERT_EQ(client.read_until(output, 2), 2);
    EXPECT_EQ(std::string(output, 2), "AB");
    EXPECT_GE(peer.sends, 2);

    // Exercise the char* and string overloads through the same hook.
    char mutable_payload[] = "C";
    ASSERT_EQ(peer.write(mutable_payload, 1), 1);
    ASSERT_EQ(peer.write(std::string("D")), 1);
    char extra[2]{};
    ASSERT_EQ(client.read_until(extra, 2), 2);
    EXPECT_EQ(std::string(extra, 2), "CD");
    ASSERT_EQ(peer.write("E"), 1);
    char mutable_terminated[] = "F";
    ASSERT_EQ(peer.write(mutable_terminated), 1);
    char more[2]{};
    ASSERT_EQ(client.read_until(more, 2), 2);
    EXPECT_EQ(std::string(more, 2), "EF");
    aeon::socket* generic = &peer;
    ASSERT_EQ(generic->write("G", 1), 1);
    char last{};
    ASSERT_EQ(client.read_until(&last, 1), 1);
    EXPECT_EQ(last, 'G');
    EXPECT_GE(peer.sends, 7);
    EXPECT_TRUE(poller.remove_socket(&peer));
}
