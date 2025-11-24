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

namespace net {


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
    if (!IsValidPort(port)) {
        this->error_code = ERR_NOSOCKET;
        this->error_state = SOCK_CONNECT;
        return false;
    }
    this->remote_host = hostname;
    this->port = port;
    return this->Connect();
}

/**
 * Connect to previously set remote host and port
 * Resolves hostname and delegates to Connect(const CAddress&)
 * \return true if setup succeeded, false otherwise
 */
bool CClientSocketUDP::Connect() {
    struct addrinfo hints, *server_info, *connection;
    std::memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    if (!IsValidSocket(this->sockfd)) {
        this->error_code = ERR_NOSOCKET;
        this->error_state = SOCK_CREATE;
        return false;
    }

    std::string port_str = std::to_string(this->port);
    int rv = getaddrinfo(this->remote_host.c_str(), port_str.c_str(), &hints, &server_info);
    if (rv != 0) {
        this->error_code = ERR_NOHOST;
        this->error_state = SOCK_RESOLVE;
        return false;
    }

    // Try each address until one succeeds
    for (connection = server_info; connection != nullptr; connection = connection->ai_next) {
        CAddress addr;
        if (connection->ai_family == AF_INET) {
            addr = CAddress(*(sockaddr_in*)connection->ai_addr);
        } else if (connection->ai_family == AF_INET6) {
            addr = CAddress(*(sockaddr_in6*)connection->ai_addr);
        } else {
            continue;
        }

        if (this->Connect(addr)) {
            freeaddrinfo(server_info);
            return true;
        }
    }

    freeaddrinfo(server_info);
    this->connected = false;
    return false;
}

/**
 * Connect to a previously resolved CAddress
 * \param addr CAddress object containing the remote address and port
 * \return true if setup succeeded, false otherwise
 * 
 * Single source of truth for UDP target setup.
 * Just stores the address for use with sendto().
 */
bool CClientSocketUDP::Connect(const CAddress& addr) {
    int target_family = addr.IsIPv6() ? AF_INET6 : AF_INET;
    
    // Recreate socket if family doesn't match
    if (target_family != this->net_family) {
        if (IsValidSocket(this->sockfd)) {
            CLOSE_SOCKET(this->sockfd);
        }
        this->sockfd = socket(target_family, CSocket::DatagramSocketType, 0);
        if (!IsValidSocket(this->sockfd)) {
            this->error_code = ERR_NOSOCKET;
            this->error_state = SOCK_CREATE;
            return false;
        }
        this->SetSocketReusAddr();
        this->net_family = target_family;
    }
    
    this->remote_addr = addr.GetSockaddrStorage();
    this->connected = true;
    return true;
}

}  // namespace net

#endif  // _CCLIENT_SOCKET_UDP_CPP