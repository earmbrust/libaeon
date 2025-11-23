/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#ifndef _CEVENT_SOCKET_CPP
#define _CEVENT_SOCKET_CPP

#include "libaeon.h"
#include <cstring>

namespace net {

// Platform-specific helper macros
#ifdef PLATFORM_WINDOWS
    #define CLOSE_SOCKET(s) closesocket(s)
    #define GET_NET_SOCKET_ERROR() WSAGetLastError()
#else
    #define CLOSE_SOCKET(s) close(s)
    #define GET_NET_SOCKET_ERROR() errno
#endif

/**
 * Poll for incoming data on the socket
 * \return The result of OnRead() callback
 * 
 * Poll() reads available data from the socket into the internal buffer,
 * then calls the OnRead() member function with the data.
 * Override OnRead() in derived classes to handle incoming data.
 */
bool CEventSocket::Poll() {
    if (!IsValidSocket(this->sockfd)) {
        return false;
    }

    SafeClearBuffer(this->inbuffer, CSocket::MaxBufferSize);
    int bytesRead = recv(this->sockfd, this->inbuffer, CSocket::MaxBufferSize, 0);
    this->n = bytesRead;

    if (bytesRead == 0) {
        this->connected = false;
    } else if (bytesRead < 0) {
        this->error_code = GET_NET_SOCKET_ERROR();
    }

    // Call the OnRead callback
    return this->OnRead(this->inbuffer, bytesRead);
}

/**
 * Write character data to the socket
 * \param data Pointer to null-terminated string to write
 * \return The number of bytes written
 * 
 * Sends data to the socket and calls the OnWrite() callback
 */
int CEventSocket::Write(char* data) {
    if (!data || !IsValidSocket(this->sockfd)) {
        return NET_SOCKET_ERROR;
    }

    std::size_t len = std::strlen(data);
    if (len == 0) {
        return 0;
    }

    int bytesSent = send(this->sockfd, data, static_cast<int>(len), 0);
    
    if (bytesSent < 0) {
        this->error_code = GET_NET_SOCKET_ERROR();
    }

    // Call the OnWrite callback
    this->OnWrite(data, static_cast<int>(len), bytesSent);
    return bytesSent;
}

/**
 * Write const character data to the socket
 * \param data Pointer to null-terminated const string to write
 * \return The number of bytes written
 */
int CEventSocket::Write(const char* data) {
    return this->Write(const_cast<char*>(data));
}

/**
 * Write std::string data to the socket
 * \param data std::string to write
 * \return The number of bytes written
 * 
 * Sends string data to the socket and calls the OnWrite() callback
 */
int CEventSocket::Write(const std::string& data) {
    if (!IsValidSocket(this->sockfd) || data.empty()) {
        return NET_SOCKET_ERROR;
    }

    int bytesSent = send(this->sockfd, data.c_str(), static_cast<int>(data.size()), 0);
    
    if (bytesSent < 0) {
        this->error_code = GET_NET_SOCKET_ERROR();
    }

    // Call the OnWrite callback
    this->OnWrite(data.c_str(), static_cast<int>(data.size()), bytesSent);
    return bytesSent;
}

/**
 * Called when data is received (virtual callback)
 * \param buffer Pointer to data buffer
 * \param size Number of bytes received
 * \return true to continue, false to stop polling
 * 
 * Override this method in derived classes to handle incoming data.
 * Return false to stop the polling mechanism.
 */
bool CEventSocket::OnRead(const char* buffer, int size) {
    (void)buffer;  // Suppress unused parameter warning
    (void)size;
    return false;
}

/**
 * Called when data is sent (virtual callback)
 * \param buffer Pointer to data that was sent
 * \param size Number of bytes requested to send
 * \param sentsize Number of bytes actually sent
 * 
 * Override this method in derived classes to handle send completion.
 */
void CEventSocket::OnWrite(const char* buffer, int size, int sentsize) {
    (void)buffer;       // Suppress unused parameter warning
    (void)size;
    (void)sentsize;
}

}  // namespace net

#endif  // _CEVENT_SOCKET_CPP