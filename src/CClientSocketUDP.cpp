/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#ifndef _CCLIENT_SOCKET_UDP_CPP
#define _CCLIENT_SOCKET_UDP_CPP

#include "libaeon.h"
#include <cstdio>
#include <cstdlib>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

namespace net {

// Platform-specific helper macros
#ifdef PLATFORM_WINDOWS
    #define CLOSE_SOCKET(s) closesocket(s)
#else
    #define CLOSE_SOCKET(s) close(s)
#endif

/**
 * CClientSocketUDP default constructor
 */
CClientSocketUDP::CClientSocketUDP() {
}

/**
 * CClientSocketUDP constructor with hostname and port
 * \param hostname Remote hostname to send to
 * \param port Remote port to send to
 */
CClientSocketUDP::CClientSocketUDP(const char* hostname, int port) {
    this->Connect(hostname, port);
}

/**
 * CClientSocketUDP constructor with std::string hostname and port
 * \param hostname Remote hostname to send to (as std::string pointer)
 * \param port Remote port to send to
 */
CClientSocketUDP::CClientSocketUDP(std::string* hostname, int port) {
    if (hostname) {
        this->Connect(hostname->c_str(), port);
    }
}

/**
 * CClientSocketUDP destructor
 */
CClientSocketUDP::~CClientSocketUDP() {
}

/**
 * Connect with hostname and port parameters
 * \param hostname The remote host to send to
 * \param port The remote port to send to
 * \return true if setup succeeded, false otherwise
 */
bool CClientSocketUDP::Connect(const char* hostname, int port) {
    this->remote_host = hostname;
    this->port = port;
    return this->Connect();
}

/**
 * Connect to previously set remote host and port
 * \return true if setup succeeded, false otherwise
 */
bool CClientSocketUDP::Connect() {
    struct addrinfo hints, *server_info, *connection;
    std::memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;      // IPv4 or IPv6
    hints.ai_socktype = SOCK_DGRAM;   // UDP
    
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

    // Use first valid address (UDP doesn't require actual connection)
    for (connection = server_info; connection != nullptr; connection = connection->ai_next) {
        // Copy the address into our remote_addr for sendto/recvfrom
        if (connection->ai_family == AF_INET) {
            std::memcpy(&this->remote_addr, connection->ai_addr, connection->ai_addrlen);
            this->net_family = AF_INET;
            this->connected = true;
            freeaddrinfo(server_info);
            return true;
        }
    }

    // No valid address found
    freeaddrinfo(server_info);
    this->connected = false;
    return false;
}

}  // namespace net

#endif  // _CCLIENT_SOCKET_UDP_CPP