/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#include <aeon.hpp>
#include "native_error.hpp"
#include <cstring>
#include <climits>

namespace aeon {


/**
 * udp_socket default constructor - creates UDP socket
 */
udp_socket::udp_socket()
    : socket(socket::default_family, socket::datagram_type) {
    if (!this->is_valid_socket()) {
        return;
    }
    this->set_socket_reuseaddr();
}

// A logical peer starts without a native descriptor. The server remains its
// exclusive transport owner. Peer destruction must never close the server fd.
udp_socket::udp_socket(udp_socket* parent, const address& endpoint)
    : socket(invalid_socket, true), parent_(parent) {
    remote_addr = endpoint.get_sockaddr_storage();
    net_family = remote_addr.ss_family;
    set_parent_server(parent);
    connected = true; // logical endpoint, not a native connect()
    error_code = err_none;
    error_state = 0;
}
udp_socket::~udp_socket() { close(); }
bool udp_socket::close() {
    if (parent_) {
        const bool was_active = peer_active_;
        peer_active_ = false;
        connected = false;
        return was_active;
    }
    return socket::close();
}
bool udp_socket::is_valid_socket() const {
    return parent_ ? peer_active_ && parent_->is_valid_socket() : socket::is_valid_socket();
}

int udp_socket::send_datagram(const char* data, int size) {
    if (!is_valid_socket() || size < 0 || (size > 0 && !data)) {
        error_code = !is_valid_socket() ? err_no_socket : err_invalid_argument;
        return NET_SOCKET_ERROR;
    }
    const socket_t native = parent_ ? parent_->sockfd : sockfd;
    sockaddr_storage target = remote_addr;
    // The parent stores the bound native socket family; no syscall per send.
    const int family = parent_ ? parent_->net_family : net_family;
    socklen_t target_len = 0;
    if (target.ss_family == family) {
        target_len = static_cast<socklen_t>(family == AF_INET6 ? sizeof(sockaddr_in6) : sizeof(sockaddr_in));
    } else if (family == AF_INET6 && target.ss_family == AF_INET) {
        const auto* v4 = reinterpret_cast<const sockaddr_in*>(&target);
        sockaddr_in6 mapped{};
        mapped.sin6_family = AF_INET6;
        mapped.sin6_port = v4->sin_port;
        mapped.sin6_addr.s6_addr[10] = 0xff;
        mapped.sin6_addr.s6_addr[11] = 0xff;
        std::memcpy(&mapped.sin6_addr.s6_addr[12], &v4->sin_addr, sizeof(v4->sin_addr));
        std::memcpy(&target, &mapped, sizeof(mapped));
        target_len = static_cast<socklen_t>(sizeof(mapped));
    } else {
        error_code = err_address_not_available;
        return NET_SOCKET_ERROR;
    }
    const char zero = 0;
    const int sent = static_cast<int>(::sendto(native, size ? data : &zero, size, 0,
        reinterpret_cast<const sockaddr*>(&target), target_len));
    error_code = sent < 0 ? translate_native_error(GET_NET_SOCKET_ERROR()) : err_none;
    n = sent;
    return sent;
}
int udp_socket::write(char* data, int size) { return send_datagram(data, size); }
int udp_socket::write(const char* data, int size) { return send_datagram(data, size); }
int udp_socket::write(char* data) {
    if (!data) return NET_SOCKET_ERROR;
    const auto n = std::strlen(data);
    return n > static_cast<std::size_t>(INT_MAX) ? NET_SOCKET_ERROR
        : send_datagram(data, static_cast<int>(n));
}
int udp_socket::write(const char* data) {
    if (!data) return NET_SOCKET_ERROR;
    const auto n = std::strlen(data);
    return n > static_cast<std::size_t>(INT_MAX) ? NET_SOCKET_ERROR
        : send_datagram(data, static_cast<int>(n));
}
int udp_socket::write(const std::string& data) {
    return data.size() > static_cast<std::size_t>(INT_MAX)
        ? NET_SOCKET_ERROR : send_datagram(data.data(), static_cast<int>(data.size()));
}

/**
 * Read data from UDP socket into buffer
 * \param buffer Character array to receive data
 * \param size Maximum number of bytes to read
 * \return Number of bytes read, 0 on timeout or empty datagram, -1 on error
 */
LIBAEON_API int udp_socket::read(char* buffer, int size) {
    if (parent_ || !buffer || !this->is_valid_socket() || size <= 0) {
        this->error_code = !this->is_valid_socket() ? err_no_socket : err_invalid_argument;
        this->n = NET_SOCKET_ERROR;
        return NET_SOCKET_ERROR;
    }

    // Datagram payloads are binary and can exceed the legacy 256-byte
    // internal buffer. recvfrom writes at most the requested capacity.
    if (this->read_timeout_ms > 0) {
        const int ready = socket::wait_for_readable(this->sockfd, this->read_timeout_ms);
        if (ready == 0) {
            this->error_code = err_timeout;
            this->n = 0;
            return 0;
        }
        if (ready < 0) {
            this->error_code = translate_native_error(GET_NET_SOCKET_ERROR());
            this->error_state = state_accept;
            this->n = NET_SOCKET_ERROR;
            return NET_SOCKET_ERROR;
        }
    }

    socklen_t sockaddr_size = sizeof(sockaddr_storage);
    const int received = static_cast<int>(::recvfrom(this->sockfd, buffer, size, 0,
                            reinterpret_cast<sockaddr*>(&this->remote_addr), &sockaddr_size));
    this->n = received;
    // A zero-length UDP datagram is valid; it is not a disconnect.
    if (received < 0) {
        this->error_code = translate_native_error(GET_NET_SOCKET_ERROR());
        this->error_state = state_accept;
    } else {
        this->error_code = err_none;
    }
    return received;
}

/**
 * Read data from UDP socket as std::string
 * \param size Maximum number of bytes to read
 * \return Binary-safe datagram data, or an empty string on timeout/error
 */
LIBAEON_API std::string udp_socket::read(int size) {
    if (!this->is_valid_socket() || size <= 0) return {};

    std::string result(static_cast<std::size_t>(size), '\0');
    const int received = this->read(result.data(), size);
    if (received <= 0) return {};
    result.resize(static_cast<std::size_t>(received));
    return result;
}

/**
 * Read data from UDP socket into internal buffer
 * \return Number of bytes read
 */
LIBAEON_API int udp_socket::read() {
    const int received = this->read(this->inbuffer, socket::max_buffer_size - 1);
    if (received >= 0) this->inbuffer[received] = '\0';
    return received;
}


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

bool udp_socket::udp_connect(const char* hostname, int port) {
    if (!hostname || !*hostname || !is_valid_port(port)) {
        this->error_code = err_no_host;
        this->error_state = state_connect;
        return false;
    }

    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    addrinfo* results = nullptr;
    const std::string service = std::to_string(port);
    if (::getaddrinfo(hostname, service.c_str(), &hints, &results) != 0) {
        this->error_code = err_no_host;
        this->error_state = state_connect;
        return false;
    }

    bool success = false;
    for (const addrinfo* current = results; current; current = current->ai_next) {
        if (!is_supported_family(current->ai_family) || !current->ai_addr ||
            current->ai_addrlen > sizeof(sockaddr_storage)) continue;
        sockaddr_storage storage{};
        std::memcpy(&storage, current->ai_addr, current->ai_addrlen);
        if (udp_connect(address(storage))) {
            this->set_remote_hostname(hostname);
            success = true;
            break;
        }
    }
    ::freeaddrinfo(results);
    if (!success) {
        this->error_code = err_no_host;
        this->error_state = state_connect;
    }
    return success;
}

bool udp_socket::udp_connect(const address& remote) {
    const sockaddr_storage target = remote.get_sockaddr_storage();
    if (!is_supported_family(target.ss_family)) {
        this->error_code = err_no_host;
        this->error_state = state_connect;
        return false;
    }
    if (!valid(this->sockfd) || target.ss_family != this->net_family) {
        socket_t candidate = invalid_socket;
        if (!select_family(target.ss_family, this->blocking, candidate)) {
            this->error_code = err_no_socket;
            this->error_state = state_create;
            return false;
        }
        // Do not discard the prior descriptor before replacement is ready.
        const socket_t prior = this->sockfd;
        this->sockfd = candidate;
        this->net_family = target.ss_family;
        close_candidate(prior);
    }
    this->remote_addr = target;
    this->port = static_cast<int>(remote.get_port());
    this->connected = true; // UDP destination selected; no TCP-style handshake.
    this->error_code = err_none;
    this->error_state = 0;
    return true;
}

bool udp_socket::udp_connect() {
    return udp_connect(address(this->remote_addr));
}

namespace {
int native_bind(socket_t fd, const sockaddr* addr, socklen_t length) {
    return ::bind(fd, addr, length);
}
} // namespace

bool udp_socket::udp_listen(int port) {
    return udp_listen(port, &native_bind);
}

bool udp_socket::udp_listen(int port, native_bind_fn bind_socket) {
    if (!bind_socket || !is_valid_port(port)) {
        this->error_code = err_no_socket;
        this->error_state = state_bind;
        return false;
    }
    // Bind candidate descriptors rather than rebinding or closing the live one.
    // Prefer dual stack when supported, otherwise retain IPv4 compatibility.
    int last_error = err_no_socket;
    for (const int family : {AF_INET6, AF_INET}) {
        socket_t candidate = invalid_socket;
        if (!select_family(family, this->blocking, candidate)) {
            last_error = translate_native_error(GET_NET_SOCKET_ERROR());
            continue;
        }

        if (family == AF_INET6) {
#ifdef IPV6_V6ONLY
            const int v6only = 0;
            if (::setsockopt(candidate, IPPROTO_IPV6, IPV6_V6ONLY,
                             reinterpret_cast<const char*>(&v6only),
                             sizeof(v6only)) != 0) {
                last_error = translate_native_error(GET_NET_SOCKET_ERROR());
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
            const socket_t prior = this->sockfd;
            this->sockfd = candidate;
            this->net_family = family;
            this->port = port;
            this->connected = false;
            this->remote_addr = {};
            this->error_code = err_none;
            this->error_state = 0;
            close_candidate(prior);
            return true;
        }
        last_error = translate_native_error(GET_NET_SOCKET_ERROR());
        close_candidate(candidate);
    }
    this->error_code = last_error;
    this->error_state = state_bind;
    return false;
}


}  // aeon