/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#ifndef _CSOCKET_UDP
#define _CSOCKET_UDP

#include "libaeon.h"
#include <cstring>

namespace net {


/**
 * CSocketUDP default constructor - creates UDP socket
 */
CSocketUDP::CSocketUDP() {
    // Parent CSocket constructor already initialized:
    // net_family, connected, blocking, n, port, error_code, error_state, token_size
    
    // Close the TCP socket created by parent constructor
    // CSocketUDP needs a UDP (datagram) socket instead
    if (IsValidSocket(this->sockfd)) {
        CLOSE_SOCKET(this->sockfd);
    }

    // Create UDP (datagram) socket
    this->sockfd = socket(CSocket::DefaultFamilyType, CSocket::DatagramSocketType, 0);
    
    if (!IsValidSocket(this->sockfd)) {
        this->error_code = ERR_NOSOCKET;
        this->error_state = SOCK_CREATE;
        return;
    }

    // PERFORMANCE: Set SO_REUSEADDR for rapid rebinding on UDP
    // Allows immediate port reuse after close (especially useful for UDP servers/clients)
    SetSocketReusAddr(this->sockfd);

    SafeClearBuffer(this->inbuffer, CSocket::MaxBufferSize);
    SafeClearBuffer(this->outbuffer, CSocket::MaxBufferSize);
}

/**
 * Write data to UDP socket with explicit size
 * \param data Pointer to data buffer
 * \param size Number of bytes to write
 * \return Number of bytes sent
 */
int CSocketUDP::Write(char* data, int size) {
    if (!data || !IsValidSocket(this->sockfd) || size <= 0) {
        return NET_SOCKET_ERROR;
    }

    socklen_t addr_len = (remote_addr.ss_family == AF_INET6) 
                        ? sizeof(struct sockaddr_in6) 
                        : sizeof(struct sockaddr_in);

    int bytesSent = static_cast<int>(sendto(this->sockfd, data, size, CSocket::NULLFlag, 
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
int CSocketUDP::Write(const char* data, int size) {
    return this->Write(const_cast<char*>(data), size);
}

/**
 * Write null-terminated string to UDP socket
 * \param data Pointer to null-terminated string
 * \return Number of bytes sent
 */
int CSocketUDP::Write(char* data) {
    if (!data || !IsValidSocket(this->sockfd)) {
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
                          CSocket::NULLFlag,
                          (struct sockaddr*)&this->remote_addr, 
                          addr_len));
    return bytesSent;
}

/**
 * Write null-terminated string to UDP socket (const char* version)
 * \param data Pointer to null-terminated string
 * \return Number of bytes sent
 */
int CSocketUDP::Write(const char* data) {
    return this->Write(const_cast<char*>(data));
}

/**
 * Write std::string to UDP socket
 * \param data std::string to write
 * \return Number of bytes sent
 */
int CSocketUDP::Write(const std::string& data) {
    if (!IsValidSocket(this->sockfd) || data.empty()) {
        return NET_SOCKET_ERROR;
    }

    socklen_t addr_len = (remote_addr.ss_family == AF_INET6) 
                        ? sizeof(struct sockaddr_in6) 
                        : sizeof(struct sockaddr_in);

    int bytesSent = static_cast<int>(sendto(this->sockfd, data.c_str(), static_cast<int>(data.size()), 
                          CSocket::NULLFlag,
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
int CSocketUDP::Read(char* buffer, int size) {
    if (!buffer || !IsValidSocket(this->sockfd) || size <= 0) {
        return NET_SOCKET_ERROR;
    }

    // Validate and clamp size to prevent buffer overflow
    // Protect against INT_MAX and unreasonably large read sizes
    int safe_size = (size > CSocket::MaxBufferSize) 
                  ? CSocket::MaxBufferSize 
                  : size;

    SafeClearBuffer(buffer, safe_size);
    socklen_t sockaddr_size = sizeof(struct sockaddr_storage);
    
    int bytesRead = static_cast<int>(recvfrom(this->sockfd, buffer, safe_size, CSocket::NULLFlag, 
                            (struct sockaddr*)&this->remote_addr, &sockaddr_size));
    
    this->n = bytesRead;
    
    if (bytesRead == 0) {
        this->connected = false;
    } else if (bytesRead < 0) {
        this->error_code = GET_NET_SOCKET_ERROR();
        this->error_state = SOCK_ACCEPT;
    }
    
    return bytesRead;
}

/**
 * Read data from UDP socket as std::string
 * \param size Maximum number of bytes to read
 * \return std::string containing read data
 */
std::string CSocketUDP::Read(int size) {
    std::string retVal;
    
    if (!IsValidSocket(this->sockfd) || size <= 0) {
        return retVal;
    }

    // Validate and clamp size to prevent buffer overflow
    // Protect against INT_MAX and values exceeding MaxBufferSize
    int safe_size = (size > CSocket::MaxBufferSize - 1) 
                  ? CSocket::MaxBufferSize - 1 
                  : size;

    SafeClearBuffer(this->inbuffer, CSocket::MaxBufferSize);
    socklen_t sockaddr_size = sizeof(struct sockaddr_storage);
    
    int bytesRead = static_cast<int>(recvfrom(this->sockfd, this->inbuffer, safe_size, CSocket::NULLFlag, 
                            (struct sockaddr*)&this->remote_addr, &sockaddr_size));
    
    this->n = bytesRead;
    
    if (bytesRead > 0) {
        this->inbuffer[bytesRead] = '\0';
        retVal = this->inbuffer;
    } else if (bytesRead == 0) {
        this->connected = false;
    } else {
        this->error_code = GET_NET_SOCKET_ERROR();
        this->error_state = SOCK_ACCEPT;
    }
    
    return retVal;
}

/**
 * Read data from UDP socket into internal buffer
 * \return Number of bytes read
 */
int CSocketUDP::Read() {
    return this->Read(this->inbuffer, CSocket::MaxBufferSize - 1);
}

}  // namespace net

#endif  // _CSOCKET_UDP