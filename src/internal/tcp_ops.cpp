#include "tcp_ops.hpp"
#include <cstring>
#include <string>

namespace aeon::internal {

bool tcp_ops::connect(socket& owner, const char* hostname, int port) {
    if (!hostname || !*hostname || !is_valid_port(port)) {
        owner.error_code = err_no_socket;
        owner.error_state = state_connect;
        return false;
    }
    owner.set_remote_hostname(hostname);
    owner.port = port;
    return connect(owner);
}

bool tcp_ops::connect(socket& owner) {
    const std::string hostname = owner.remote_hostname();
    if (hostname.empty() || !is_valid_port(owner.port)) {
        owner.error_code = err_no_host;
        owner.error_state = state_resolve;
        return false;
    }
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* results = nullptr;
    const std::string port_string = std::to_string(owner.port);
    if (::getaddrinfo(hostname.c_str(), port_string.c_str(),
                      &hints, &results) != 0) {
        owner.error_code = err_no_host;
        owner.error_state = state_resolve;
        return false;
    }
    bool success = false;
    for (addrinfo* entry = results; entry; entry = entry->ai_next) {
        if (entry->ai_family != AF_INET && entry->ai_family != AF_INET6) continue;
        if (entry->ai_family == AF_INET) {
            success = connect(owner, address(*reinterpret_cast<sockaddr_in*>(entry->ai_addr)));
        } else {
            success = connect(owner, address(*reinterpret_cast<sockaddr_in6*>(entry->ai_addr)));
        }
        if (success) break;
    }
    ::freeaddrinfo(results);
    owner.connected = success;
    return success;
}

bool tcp_ops::connect(socket& owner, const address& addr) {
    const sockaddr_storage target = addr.get_sockaddr_storage();
    const int family = target.ss_family;
    if (family != AF_INET && family != AF_INET6) {
        owner.error_code = err_no_socket;
        owner.error_state = state_connect;
        return false;
    }
    const socklen_t length = family == AF_INET6
        ? static_cast<socklen_t>(sizeof(sockaddr_in6))
        : static_cast<socklen_t>(sizeof(sockaddr_in));

    // A fresh descriptor for each attempt prevents stale TCP state on retry.
    owner.close();
    owner.sockfd = ::socket(family, SOCK_STREAM, 0);
    owner.connected = false;
    if (!owner.is_valid_socket()) {
        owner.error_code = err_no_socket;
        owner.error_state = state_create;
        return false;
    }
    owner.net_family = family;
    owner.configure_socket_for_connect();

    bool succeeded = false;
    if (owner.connect_timeout_ms > 0) {
        blocking_mode_guard guard(&owner);
        if (!guard.is_valid()) {
            owner.error_code = GET_NET_SOCKET_ERROR();
            owner.error_state = state_connect;
        } else {
            const int status = ::connect(owner.sockfd,
                reinterpret_cast<const sockaddr*>(&target), length);
            if (status == 0) {
                succeeded = true;
            } else {
                const int error = GET_NET_SOCKET_ERROR();
#ifdef NET_PLATFORM_WINDOWS
                const bool pending = error == WSAEWOULDBLOCK || error == WSAEINPROGRESS;
#else
                const bool pending = error == EINPROGRESS || error == EWOULDBLOCK;
#endif
                if (pending) {
                    const int ready = socket::wait_for_writable(owner.sockfd,
                        owner.connect_timeout_ms);
                    if (ready > 0) {
                        int so_error = 0;
#ifdef NET_PLATFORM_WINDOWS
                        int len = static_cast<int>(sizeof(so_error));
#else
                        socklen_t len = sizeof(so_error);
#endif
                        if (::getsockopt(owner.sockfd, SOL_SOCKET, SO_ERROR,
                            reinterpret_cast<char*>(&so_error), &len) != 0) {
                            owner.error_code = GET_NET_SOCKET_ERROR();
                            owner.error_state = state_connect;
                        } else if (so_error == 0) {
                            succeeded = true;
                        } else {
                            owner.error_code = so_error;
                            owner.error_state = state_connect;
                        }
                    } else if (ready == 0) {
#ifdef NET_PLATFORM_WINDOWS
                        owner.error_code = WSAETIMEDOUT;
#else
                        owner.error_code = ETIMEDOUT;
#endif
                        owner.error_state = state_connect;
                    } else {
                        owner.error_code = GET_NET_SOCKET_ERROR();
                        owner.error_state = state_connect;
                    }
                } else {
                    owner.error_code = error;
                    owner.error_state = state_connect;
                }
            }
        }
    } else {
        if (::connect(owner.sockfd,
            reinterpret_cast<const sockaddr*>(&target), length) == 0) {
            succeeded = true;
        } else {
            owner.error_code = GET_NET_SOCKET_ERROR();
            owner.error_state = state_connect;
        }
    }

    if (!succeeded) {
        owner.close();
        return false;
    }
    owner.remote_addr = target;
    owner.connected = true;
    owner.clear_buffers();
    owner.error_code = err_none;
    owner.error_state = 0;
    return true;
}

int tcp_ops::bind(socket& owner, const sockaddr* addr, socklen_t length) {
    if (!owner.is_valid_socket() || !addr) {
        owner.error_code = err_no_socket;
        owner.error_state = state_bind;
        return -1;
    }
    if (::bind(owner.sockfd, addr, length) != 0) {
        owner.error_code = GET_NET_SOCKET_ERROR();
        owner.error_state = state_bind;
        return -1;
    }
    owner.error_code = err_none;
    return 0;
}

bool tcp_ops::listen(socket& owner, const char* ip, int port) {
    if (!ip || !is_valid_port(port)) {
        owner.error_code = err_no_socket;
        owner.error_state = state_bind;
        return false;
    }
    sockaddr_storage target{};
    socklen_t length = 0;
    auto* ip4 = reinterpret_cast<sockaddr_in*>(&target);
    auto* ip6 = reinterpret_cast<sockaddr_in6*>(&target);
    if (::inet_pton(AF_INET6, ip, &ip6->sin6_addr) == 1) {
        ip6->sin6_family = AF_INET6;
        ip6->sin6_port = htons(static_cast<unsigned short>(port));
        length = static_cast<socklen_t>(sizeof(sockaddr_in6));
    } else if (::inet_pton(AF_INET, ip, &ip4->sin_addr) == 1) {
        ip4->sin_family = AF_INET;
        ip4->sin_port = htons(static_cast<unsigned short>(port));
        length = static_cast<socklen_t>(sizeof(sockaddr_in));
    } else {
        owner.error_code = err_no_socket;
        owner.error_state = state_bind;
        return false;
    }
    // Listening is always a new binding attempt. Never reuse a previously
    // listening descriptor, including when rebinding within the same family.
    owner.close();
    owner.sockfd = ::socket(target.ss_family, SOCK_STREAM, 0);
    if (!owner.is_valid_socket()) {
        owner.error_code = err_no_socket;
        owner.error_state = state_create;
        return false;
    }
    owner.net_family = target.ss_family;
    owner.port = port;
    owner.set_so_reuseaddr(true);
    if (target.ss_family == AF_INET6) owner.set_ipv6_v6only(true);
    if (bind(owner, reinterpret_cast<const sockaddr*>(&target), length) != 0) {
        owner.close();
        return false;
    }
    if (::listen(owner.sockfd, SOMAXCONN) != 0) {
        owner.error_code = GET_NET_SOCKET_ERROR();
        owner.error_state = state_accept;
        owner.close();
        return false;
    }
    owner.connected = true;
    owner.error_state = 0;
    owner.error_code = err_none;
    return true;
}

socket_t tcp_ops::accept(socket& listener, int timeout_ms, bool blocking,
                         sockaddr_storage& peer) {
    if (!listener.is_valid_socket()) {
        listener.error_code = err_no_socket;
        listener.error_state = state_accept;
        return invalid_socket;
    }
    if (timeout_ms > 0) {
        const int ready = socket::wait_for_readable(listener.sockfd, timeout_ms);
        if (ready == 0) return invalid_socket; // timeout is not an error
        if (ready < 0) {
            listener.error_code = GET_NET_SOCKET_ERROR();
            listener.error_state = state_accept;
            return invalid_socket;
        }
    }
    socklen_t length = static_cast<socklen_t>(sizeof(peer));
    const socket_t fd = ::accept(listener.sockfd,
        reinterpret_cast<sockaddr*>(&peer), &length);
    if (fd == invalid_socket) {
        const int error = GET_NET_SOCKET_ERROR();
#ifdef NET_PLATFORM_WINDOWS
        if (error == WSAEWOULDBLOCK || error == WSAECONNRESET) return invalid_socket;
#else
        if (error == EAGAIN || error == EWOULDBLOCK || error == ECONNRESET) return invalid_socket;
#endif
        listener.error_code = error;
        listener.error_state = state_accept;
        return invalid_socket;
    }
    // Configure the accepted descriptor before transferring ownership.
#ifdef NET_PLATFORM_WINDOWS
    u_long mode = blocking ? 0UL : 1UL;
    if (::ioctlsocket(fd, FIONBIO, &mode) != 0) {
        listener.error_code = GET_NET_SOCKET_ERROR();
        listener.error_state = state_accept;
        NET_CLOSE_SOCKET(fd);
        return invalid_socket;
    }
#else
    const int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0 || ::fcntl(fd, F_SETFL,
                           blocking ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK)) != 0) {
        listener.error_code = GET_NET_SOCKET_ERROR();
        listener.error_state = state_accept;
        NET_CLOSE_SOCKET(fd);
        return invalid_socket;
    }
#endif
    int nodelay = 1;
    (void)::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY,
                       reinterpret_cast<const char*>(&nodelay), sizeof(nodelay));
    listener.error_code = err_none;
    listener.error_state = 0;
    return fd;
}

} // namespace aeon::internal
