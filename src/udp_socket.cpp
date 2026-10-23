/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#include <aeon.hpp>
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
    if (!is_valid_socket() || size < 0 || (size > 0 && !data)) return NET_SOCKET_ERROR;
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
    } else return NET_SOCKET_ERROR;
    const char zero = 0;
    const int sent = static_cast<int>(::sendto(native, size ? data : &zero, size, 0,
        reinterpret_cast<const sockaddr*>(&target), target_len));
    if (sent < 0) error_code = GET_NET_SOCKET_ERROR();
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
        this->n = NET_SOCKET_ERROR;
        return NET_SOCKET_ERROR;
    }

    // Datagram payloads are binary and can exceed the legacy 256-byte
    // internal buffer. recvfrom writes at most the requested capacity.
    if (this->read_timeout_ms > 0) {
        const int ready = socket::wait_for_readable(this->sockfd, this->read_timeout_ms);
        if (ready == 0) {
            this->n = 0;
            return 0;
        }
        if (ready < 0) {
            this->error_code = GET_NET_SOCKET_ERROR();
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
        this->error_code = GET_NET_SOCKET_ERROR();
        this->error_state = state_accept;
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
}  // aeon