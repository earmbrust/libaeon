/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#include <aeon.hpp>
#include <cstring>

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