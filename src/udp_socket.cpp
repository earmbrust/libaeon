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

/**
 * Write data to UDP socket with explicit size
 * \param data Pointer to data buffer
 * \param size Number of bytes to write
 * \return Number of bytes sent
 */
LIBAEON_API int udp_socket::write(char* data, int size) {
    if (!data || !this->is_valid_socket() || size <= 0) {
        return NET_SOCKET_ERROR;
    }

    socklen_t addr_len = (remote_addr.ss_family == AF_INET6) 
                        ? sizeof(struct sockaddr_in6) 
                        : sizeof(struct sockaddr_in);

    int bytesSent = static_cast<int>(sendto(this->sockfd, data, size, 0, 
                          (struct sockaddr*)&this->remote_addr, 
                          addr_len));
    return bytesSent;
}

/**
 * Write data to UDP socket with explicit size (const char* version)
 * \param data Pointer to data buffer
 * \param size Number of bytes to write
 * \return Number of bytes sent
 */
LIBAEON_API int udp_socket::write(const char* data, int size) {
    return this->write(const_cast<char*>(data), size);
}

/**
 * Write null-terminated string to UDP socket
 * \param data Pointer to null-terminated string
 * \return Number of bytes sent
 */
LIBAEON_API int udp_socket::write(char* data) {
    if (!data || !this->is_valid_socket()) {
        return NET_SOCKET_ERROR;
    }

    std::size_t len = std::strlen(data);
    if (len == 0) {
        return 0;
    }

    socklen_t addr_len = (remote_addr.ss_family == AF_INET6) 
                        ? sizeof(struct sockaddr_in6) 
                        : sizeof(struct sockaddr_in);

    int bytesSent = static_cast<int>(sendto(this->sockfd, data, static_cast<int>(len), 
                          0,
                          (struct sockaddr*)&this->remote_addr, 
                          addr_len));
    return bytesSent;
}

/**
 * Write null-terminated string to UDP socket (const char* version)
 * \param data Pointer to null-terminated string
 * \return Number of bytes sent
 */
LIBAEON_API int udp_socket::write(const char* data) {
    return this->write(const_cast<char*>(data));
}

/**
 * Write std::string to UDP socket
 * \param data std::string to write
 * \return Number of bytes sent
 */
LIBAEON_API int udp_socket::write(const std::string& data) {
    if (!this->is_valid_socket() || data.empty()) {
        return NET_SOCKET_ERROR;
    }

    socklen_t addr_len = (remote_addr.ss_family == AF_INET6) 
                        ? sizeof(struct sockaddr_in6) 
                        : sizeof(struct sockaddr_in);

    int bytesSent = static_cast<int>(sendto(this->sockfd, data.c_str(), static_cast<int>(data.size()), 
                          0,
                          (struct sockaddr*)&this->remote_addr, 
                          addr_len));
    return bytesSent;
}

LIBAEON_API int udp_socket::write_to(const address& destination, const char* data, int size) {
    if (!is_valid_socket() || size < 0 || (size > 0 && !data)) return NET_SOCKET_ERROR;

    sockaddr_storage target = destination.get_sockaddr_storage();
    socklen_t target_len = 0;
    if (target.ss_family == net_family) {
        target_len = static_cast<socklen_t>(net_family == AF_INET6
            ? sizeof(sockaddr_in6) : sizeof(sockaddr_in));
    } else if (net_family == AF_INET6 && target.ss_family == AF_INET) {
        // A dual-stack IPv6 listener needs an IPv4-mapped destination.
        const auto* ipv4 = reinterpret_cast<const sockaddr_in*>(&target);
        sockaddr_in6 mapped{};
        mapped.sin6_family = AF_INET6;
        mapped.sin6_port = ipv4->sin_port;
        mapped.sin6_addr.s6_addr[10] = 0xff;
        mapped.sin6_addr.s6_addr[11] = 0xff;
        std::memcpy(&mapped.sin6_addr.s6_addr[12], &ipv4->sin_addr, sizeof(ipv4->sin_addr));
        std::memcpy(&target, &mapped, sizeof(mapped));
        target_len = static_cast<socklen_t>(sizeof(mapped));
    } else {
        return NET_SOCKET_ERROR;
    }
    // The native API permits a null buffer for a zero-byte datagram on some
    // systems but not all; provide a stable non-null pointer instead.
    const char empty = 0;
    const int sent = static_cast<int>(::sendto(sockfd, size ? data : &empty, size, 0,
        reinterpret_cast<const sockaddr*>(&target), target_len));
    if (sent < 0) error_code = GET_NET_SOCKET_ERROR();
    return sent;
}

LIBAEON_API int udp_socket::write_to(const address& destination, const std::string& data) {
    if (data.size() > static_cast<std::size_t>(INT_MAX)) return NET_SOCKET_ERROR;
    return write_to(destination, data.data(), static_cast<int>(data.size()));
}

/**
 * Read data from UDP socket into buffer
 * \param buffer Character array to receive data
 * \param size Maximum number of bytes to read
 * \return Number of bytes read, 0 on timeout or empty datagram, -1 on error
 */
LIBAEON_API int udp_socket::read(char* buffer, int size) {
    if (!buffer || !this->is_valid_socket() || size <= 0) {
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