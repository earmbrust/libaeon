#include "udp_ops.hpp"
#include "internal/config.hpp"
#include "internal/platform.hpp"

#include <cstring>
#include <string>

namespace aeon::internal {
namespace {

bool valid(socket_t fd) {
    return fd != invalid_socket;
}

void close_candidate(socket_t fd) {
    if (valid(fd)) {
        NET_CLOSE_SOCKET(fd);
    }
}

bool configure_blocking(socket_t fd, bool blocking) {
#ifdef NET_PLATFORM_WINDOWS
    u_long nonblocking = blocking ? 0UL : 1UL;
    return ioctlsocket(fd, FIONBIO, &nonblocking) == 0;
#else
    const int flags = fcntl(fd, F_GETFL, 0);
    return flags >= 0 &&
           fcntl(fd, F_SETFL, blocking ? (flags & ~O_NONBLOCK)
                                      : (flags | O_NONBLOCK)) == 0;
#endif
}

bool is_supported_family(int family) {
    return family == AF_INET || family == AF_INET6;
}

socklen_t address_length(int family) {
    return static_cast<socklen_t>(family == AF_INET6 ? sizeof(sockaddr_in6)
                                                     : sizeof(sockaddr_in));
}

// The existing destination is unchanged when the candidate cannot be created.
// A successful swap closes the previous descriptor exactly once.
bool select_family(int family, bool blocking, socket_t& candidate) {
    candidate = invalid_socket;
    if (!is_supported_family(family)) return false;
    candidate = ::socket(family, SOCK_DGRAM, 0);
    if (!valid(candidate)) return false;
    if (!configure_blocking(candidate, blocking)) {
        close_candidate(candidate);
        candidate = invalid_socket;
        return false;
    }
    return true;
}

} // namespace

bool udp_ops::connect(socket& owner, const char* hostname, int port) {
    if (!hostname || !*hostname || !is_valid_port(port)) {
        owner.error_code = err_no_host;
        owner.error_state = state_connect;
        return false;
    }

    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    addrinfo* results = nullptr;
    const std::string service = std::to_string(port);
    if (::getaddrinfo(hostname, service.c_str(), &hints, &results) != 0) {
        owner.error_code = err_no_host;
        owner.error_state = state_connect;
        return false;
    }

    bool success = false;
    for (const addrinfo* current = results; current; current = current->ai_next) {
        if (!is_supported_family(current->ai_family) || !current->ai_addr ||
            current->ai_addrlen > sizeof(sockaddr_storage)) continue;
        sockaddr_storage storage{};
        std::memcpy(&storage, current->ai_addr, current->ai_addrlen);
        if (udp_ops::connect(owner, address(storage))) {
            owner.remote_host = hostname;
            success = true;
            break;
        }
    }
    ::freeaddrinfo(results);
    if (!success) {
        owner.error_code = err_no_host;
        owner.error_state = state_connect;
    }
    return success;
}

bool udp_ops::connect(socket& owner, const address& remote) {
    const sockaddr_storage target = remote.get_sockaddr_storage();
    if (!is_supported_family(target.ss_family)) {
        owner.error_code = err_no_host;
        owner.error_state = state_connect;
        return false;
    }
    if (!valid(owner.sockfd) || target.ss_family != owner.net_family) {
        socket_t candidate = invalid_socket;
        if (!select_family(target.ss_family, owner.blocking, candidate)) {
            owner.error_code = err_no_socket;
            owner.error_state = state_create;
            return false;
        }
        // Do not discard the prior descriptor before replacement is ready.
        const socket_t prior = owner.sockfd;
        owner.sockfd = candidate;
        owner.net_family = target.ss_family;
        close_candidate(prior);
    }
    owner.remote_addr = target;
    owner.port = static_cast<int>(remote.get_port());
    owner.connected = true; // UDP destination selected; no TCP-style handshake.
    owner.error_code = err_none;
    owner.error_state = 0;
    return true;
}

bool udp_ops::connect(socket& owner) {
    return udp_ops::connect(owner, address(owner.remote_addr));
}

namespace {
int native_bind(socket_t fd, const sockaddr* addr, socklen_t length) {
    return ::bind(fd, addr, length);
}
} // namespace

bool udp_ops::listen(socket& owner, int port) {
    return listen(owner, port, &native_bind);
}

bool udp_ops::listen(socket& owner, int port, native_bind_fn bind_socket) {
    if (!bind_socket || !is_valid_port(port)) {
        owner.error_code = err_no_socket;
        owner.error_state = state_bind;
        return false;
    }
    // Bind candidate descriptors rather than rebinding or closing the live one.
    // Prefer dual stack when supported, otherwise retain IPv4 compatibility.
    int last_error = err_no_socket;
    for (const int family : {AF_INET6, AF_INET}) {
        socket_t candidate = invalid_socket;
        if (!select_family(family, owner.blocking, candidate)) {
            last_error = GET_NET_SOCKET_ERROR();
            continue;
        }

        if (family == AF_INET6) {
#ifdef IPV6_V6ONLY
            const int v6only = 0;
            if (::setsockopt(candidate, IPPROTO_IPV6, IPV6_V6ONLY,
                             reinterpret_cast<const char*>(&v6only),
                             sizeof(v6only)) != 0) {
                last_error = GET_NET_SOCKET_ERROR();
                close_candidate(candidate);
                continue;
            }
#else
            close_candidate(candidate);
            continue;
#endif
        }
        sockaddr_storage storage{};
        if (family == AF_INET6) {
            auto* v6 = reinterpret_cast<sockaddr_in6*>(&storage);
            v6->sin6_family = AF_INET6;
            v6->sin6_addr = in6addr_any;
            v6->sin6_port = htons(static_cast<unsigned short>(port));
        } else {
            auto* v4 = reinterpret_cast<sockaddr_in*>(&storage);
            v4->sin_family = AF_INET;
            v4->sin_addr.s_addr = htonl(INADDR_ANY);
            v4->sin_port = htons(static_cast<unsigned short>(port));
        }
        const socklen_t len = address_length(family);
        if (bind_socket(candidate, reinterpret_cast<const sockaddr*>(&storage), len) == 0) {
            const socket_t prior = owner.sockfd;
            owner.sockfd = candidate;
            owner.net_family = family;
            owner.port = port;
            owner.connected = false;
            owner.remote_addr = {};
            owner.error_code = err_none;
            owner.error_state = 0;
            close_candidate(prior);
            return true;
        }
        last_error = GET_NET_SOCKET_ERROR();
        close_candidate(candidate);
    }
    owner.error_code = last_error;
    owner.error_state = state_bind;
    return false;
}

} // namespace aeon::internal
