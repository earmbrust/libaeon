/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#ifndef _CCLIENT_SOCKET_CPP
#define _CCLIENT_SOCKET_CPP

#include "libaeon.h"
#include <cstdio>
#include <cstdlib>

namespace net {

// Platform-specific helper macros
#ifdef PLATFORM_WINDOWS
    #define CLOSE_SOCKET(s) closesocket(s)
#else
    #define CLOSE_SOCKET(s) close(s)
#endif

// Helper: Convert addrinfo.ai_addrlen (size_t on Windows, socklen_t on POSIX) to socklen_t
// On Windows, ai_addrlen is size_t; on POSIX it's socklen_t. This handles both.
static inline socklen_t GetAddrLen(size_t len) {
    // Safe: address lengths for IPv4/IPv6 are always < 256 bytes
    return static_cast<socklen_t>(len);
}

/**
 * Connect with hostname and port parameters
 * \param hostname The remote host to connect to
 * \param remote_port The remote port to connect to
 * \return true if connection succeeded, false otherwise
 */
bool CClientSocket::Connect(const char* hostname, int remote_port) {
    this->remote_host = hostname;
    this->port = remote_port;
    return this->Connect();
}

/**
 * Connect to previously set remote host and port
 * \return true if connection succeeded, false otherwise
 */
bool CClientSocket::Connect() {
    struct addrinfo hints, *server_info, *connection;
    std::memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;      // IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;  // TCP
    
    int rv;

    // Validate socket
    if (!IsValidSocket(this->sockfd)) {
        this->error_code = ERR_NOSOCKET;
        this->error_state = SOCK_CREATE;
        return false;
    }

    // Get address info for the host
    std::string port_str = std::to_string(this->port);
    rv = getaddrinfo(this->remote_host.c_str(), port_str.c_str(), &hints, &server_info);
    if (rv != 0) {
        this->error_code = ERR_NOHOST;
        this->error_state = SOCK_RESOLVE;
        return false;
    }

    // Try each address until we succeed
    for (connection = server_info; connection != nullptr; connection = connection->ai_next) {
        // Create socket for this address family
        socket_t sock = socket(connection->ai_family, connection->ai_socktype,
                              connection->ai_protocol);
        
        if (!IsValidSocket(sock)) {
            continue;  // Try next address
        }

        this->sockfd = sock;

        // Attempt connection
        if (connect(this->sockfd, connection->ai_addr, GetAddrLen(connection->ai_addrlen)) == 0) {
            // Success
            this->connected = true;
            this->net_family = connection->ai_family;
            freeaddrinfo(server_info);
            this->ClearBuffers();
            return true;
        }

        // Connection failed, close socket and try next
        CLOSE_SOCKET(this->sockfd);
        this->sockfd = INVALID_SOCKET_T;
    }

    // Failed to connect with any address
    freeaddrinfo(server_info);
    this->connected = false;
    return false;
}

/**
 * CClientSocket default constructor
 */
CClientSocket::CClientSocket() {
    this->sockfd = socket(CSocket::DefaultFamilyType, CSocket::StreamSocketType, 0);
}

/**
 * CClientSocket constructor with hostname and port
 * \param hostname Remote hostname to connect to
 * \param port Remote port to connect to
 */
CClientSocket::CClientSocket(const char* hostname, int port) {
    this->sockfd = socket(CSocket::DefaultFamilyType, CSocket::StreamSocketType, 0);
    this->Connect(hostname, port);
}

/**
 * CClientSocket constructor with std::string hostname and port
 * \param hostname Remote hostname to connect to (as std::string pointer)
 * \param port Remote port to connect to
 */
CClientSocket::CClientSocket(std::string* hostname, int port) {
    this->sockfd = socket(CSocket::DefaultFamilyType, CSocket::StreamSocketType, 0);
    if (hostname) {
        this->Connect(hostname->c_str(), port);
    }
}

/**
 * CClientSocket destructor
 */
CClientSocket::~CClientSocket() {
}

}  // namespace net

#endif  // _CCLIENT_SOCKET_CPP