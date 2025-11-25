/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#include <net.h>
#include <cstring>

namespace net {


/**
 * udp_socket default constructor - creates UDP socket
 */
udp_socket::udp_socket() {
    // Parent socket constructor already initialized:
    // net_family, connected, blocking, n, port, error_code, error_state, token_size
    
    // Close the TCP socket created by parent constructor
    // udp_socket needs a UDP (datagram) socket instead
    if (this->is_valid_socket()) {
        NET_CLOSE_SOCKET(this->sockfd);
    }

    // Create UDP (datagram) socket
    this->sockfd = ::socket(socket::default_family, socket::datagram_type, 0);
    
    if (!this->is_valid_socket()) {
        this->error_code = err_no_socket;
        this->error_state = state_create;
        return;
    }

    // PERFORMANCE: Set SO_REUSEADDR for rapid rebinding on UDP
    // Allows immediate port reuse after close (especially useful for UDP servers/clients)
    this->set_socket_reuseaddr();

    this->safe_clear_buffer(this->inbuffer, socket::max_buffer_size);
    this->safe_clear_buffer(this->outbuffer, socket::max_buffer_size);
}

/**
 * Write data to UDP socket with explicit size
 * \param data Pointer to data buffer
 * \param size Number of bytes to write
 * \return Number of bytes sent
 */
int udp_socket::write(char* data, int size) {
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
int udp_socket::write(const char* data, int size) {
    return this->write(const_cast<char*>(data), size);
}

/**
 * Write null-terminated string to UDP socket
 * \param data Pointer to null-terminated string
 * \return Number of bytes sent
 */
int udp_socket::write(char* data) {
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
int udp_socket::write(const char* data) {
    return this->write(const_cast<char*>(data));
}

/**
 * Write std::string to UDP socket
 * \param data std::string to write
 * \return Number of bytes sent
 */
int udp_socket::write(const std::string& data) {
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
 * \return Number of bytes read
 */
int udp_socket::read(char* buffer, int size) {
    if (!buffer || !this->is_valid_socket() || size <= 0) {
        return NET_SOCKET_ERROR;
    }

    // Validate and clamp size to prevent buffer overflow
    // Protect against INT_MAX and unreasonably large read sizes
    int safe_size = (size > socket::max_buffer_size) 
                  ? socket::max_buffer_size 
                  : size;

    this->safe_clear_buffer(buffer, safe_size);
    socklen_t sockaddr_size = sizeof(struct sockaddr_storage);
    
    int bytesRead = static_cast<int>(recvfrom(this->sockfd, buffer, safe_size, 0, 
                            (struct sockaddr*)&this->remote_addr, &sockaddr_size));
    
    this->n = bytesRead;
    
    if (bytesRead == 0) {
        this->connected = false;
    } else if (bytesRead < 0) {
        this->error_code = GET_NET_SOCKET_ERROR();
        this->error_state = state_accept;
    }
    
    return bytesRead;
}

/**
 * Read data from UDP socket as std::string
 * \param size Maximum number of bytes to read
 * \return std::string containing read data
 */
std::string udp_socket::read(int size) {
    std::string retVal;
    
    if (!this->is_valid_socket() || size <= 0) {
        return retVal;
    }

    // Validate and clamp size to prevent buffer overflow
    // Protect against INT_MAX and values exceeding MaxBufferSize
    int safe_size = (size > socket::max_buffer_size - 1) 
                  ? socket::max_buffer_size - 1 
                  : size;

    this->safe_clear_buffer(this->inbuffer, socket::max_buffer_size);
    socklen_t sockaddr_size = sizeof(struct sockaddr_storage);
    
    int bytesRead = static_cast<int>(recvfrom(this->sockfd, this->inbuffer, safe_size, 0, 
                            (struct sockaddr*)&this->remote_addr, &sockaddr_size));
    
    this->n = bytesRead;
    
    if (bytesRead > 0) {
        this->inbuffer[bytesRead] = '\0';
        retVal = this->inbuffer;
    } else if (bytesRead == 0) {
        this->connected = false;
    } else {
        this->error_code = GET_NET_SOCKET_ERROR();
        this->error_state = state_accept;
    }
    
    return retVal;
}

/**
 * Read data from UDP socket into internal buffer
 * \return Number of bytes read
 */
int udp_socket::read() {
    return this->read(this->inbuffer, socket::max_buffer_size - 1);
}

}  // namespace net