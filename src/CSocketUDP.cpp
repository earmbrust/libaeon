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

// Platform-specific helper macros
#ifdef PLATFORM_WINDOWS
    #define CLOSE_SOCKET(s) closesocket(s)
#else
    #define CLOSE_SOCKET(s) close(s)
#endif

/**
 * CSocketUDP default constructor - creates UDP socket
 */
CSocketUDP::CSocketUDP() {
#ifdef PLATFORM_WINDOWS
    this->wsaret = WSAStartup(MAKEWORD(2, 2), &wsadata);
#endif

    this->net_family = CSocket::DefaultFamilyType;
    this->connected = false;
    this->blocking = true;
    this->n = 0;
    this->port = 0;
    this->error_code = ERR_NONE;
    this->error_state = 0;
    this->token_size = 0;

    this->sockfd = socket(CSocket::DefaultFamilyType, CSocket::DatagramSocketType, 0);
    
    if (!IsValidSocket(this->sockfd)) {
        this->error_code = ERR_NOSOCKET;
        this->error_state = SOCK_CREATE;
        return;
    }

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
        return SOCKET_ERROR;
    }

    int bytesSent = sendto(this->sockfd, data, size, CSocket::NULLFlag, 
                          (struct sockaddr*)&this->remote_addr, 
                          sizeof(struct sockaddr_in));
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
        return SOCKET_ERROR;
    }

    std::size_t len = std::strlen(data);
    if (len == 0) {
        return 0;
    }

    int bytesSent = sendto(this->sockfd, data, static_cast<int>(len), 
                          CSocket::NULLFlag,
                          (struct sockaddr*)&this->remote_addr, 
                          sizeof(struct sockaddr_in));
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
int CSocketUDP::Write(std::string data) {
    if (!IsValidSocket(this->sockfd) || data.empty()) {
        return SOCKET_ERROR;
    }

    int bytesSent = sendto(this->sockfd, data.c_str(), static_cast<int>(data.size()), 
                          CSocket::NULLFlag,
                          (struct sockaddr*)&this->remote_addr, 
                          sizeof(struct sockaddr_in));
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
        return SOCKET_ERROR;
    }

    SafeClearBuffer(buffer, size);
    socklen_t sockaddr_size = sizeof(struct sockaddr_in);
    
    int bytesRead = recvfrom(this->sockfd, buffer, size, CSocket::NULLFlag, 
                            (struct sockaddr*)&this->remote_addr, &sockaddr_size);
    
    this->n = bytesRead;
    
    if (bytesRead == 0) {
        this->connected = false;
    } else if (bytesRead < 0) {
        this->error_code = GET_SOCKET_ERROR();
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

    SafeClearBuffer(this->inbuffer, CSocket::MaxBufferSize);
    socklen_t sockaddr_size = sizeof(struct sockaddr_in);
    
    int bytesRead = recvfrom(this->sockfd, this->inbuffer, size, CSocket::NULLFlag, 
                            (struct sockaddr*)&this->remote_addr, &sockaddr_size);
    
    this->n = bytesRead;
    
    if (bytesRead > 0) {
        this->inbuffer[bytesRead] = '\0';
        retVal = this->inbuffer;
    } else if (bytesRead == 0) {
        this->connected = false;
    } else {
        this->error_code = GET_SOCKET_ERROR();
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