/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#ifndef _CSERVER_SOCKET_UDP_CPP
#define _CSERVER_SOCKET_UDP_CPP

#include "libaeon.h"
#include <cstring>

namespace net {

/**
 * CServerSocketUDP default constructor
 */
CServerSocketUDP::CServerSocketUDP() {
}

/**
 * CServerSocketUDP destructor
 */
CServerSocketUDP::~CServerSocketUDP() {
}

/**
 * Bind to specified port for receiving UDP datagrams
 * \param port Port number to bind to
 * \return true if successful, false otherwise
 */
bool CServerSocketUDP::Listen(int port) {
    this->port = port;

    // UDP socket already created in CSocketUDP constructor (as IPv4)
    if (!IsValidSocket(this->sockfd)) {
        this->error_code = ERR_NOSOCKET;
        this->error_state = SOCK_CREATE;
        return false;
    }

    // Validate port range
    if (!IsValidPort(port)) {
        this->error_code = ERR_NOSOCKET;
        this->error_state = SOCK_BIND;
        return false;
    }

    // Try IPv6 first with dual-stack mode (can accept both IPv4 and IPv6)
    // If that fails, fall back to IPv4
    
    // Try to create and bind IPv6 socket first
    socket_t ipv6_sock = socket(AF_INET6, CSocket::DatagramSocketType, 0);
    if (IsValidSocket(ipv6_sock)) {
        // Try to enable dual-stack mode (accepts both IPv4 and IPv6)
        // This is not available on all platforms, so we ignore failures
#ifdef IPV6_V6ONLY
        int v6only = 0;
        setsockopt(ipv6_sock, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&v6only, sizeof(v6only));
#endif

        // Set up IPv6 address structure
        std::memset(&this->serv_addr, 0, sizeof(this->serv_addr));
        struct sockaddr_in6* addr6 = (struct sockaddr_in6*)&this->serv_addr;
        addr6->sin6_family = AF_INET6;
        addr6->sin6_addr = in6addr_any;  // Listen on any interface
        addr6->sin6_port = htons(static_cast<u_short>(port));

        // Try to bind IPv6 socket
        int bind_result = bind(ipv6_sock, 
                              (struct sockaddr*)&this->serv_addr, 
                              sizeof(struct sockaddr_in6));
        
        if (bind_result == 0) {
            // IPv6 bind succeeded - close old IPv4 socket and use IPv6
            CLOSE_SOCKET(this->sockfd);
            this->sockfd = ipv6_sock;
            this->connected = true;
            return true;
        } else {
            // IPv6 bind failed, close it and try IPv4
            CLOSE_SOCKET(ipv6_sock);
        }
    }

    // Fall back to IPv4
    // Set up IPv4 address structure
    std::memset(&this->serv_addr, 0, sizeof(this->serv_addr));
    struct sockaddr_in* addr4 = (struct sockaddr_in*)&this->serv_addr;
    addr4->sin_family = AF_INET;
    addr4->sin_addr.s_addr = htonl(INADDR_ANY);  // Listen on any interface
    addr4->sin_port = htons(static_cast<u_short>(port));

    // Bind IPv4 socket
    int bind_result = bind(this->sockfd, 
                          (struct sockaddr*)&this->serv_addr, 
                          sizeof(struct sockaddr_in));
    
    if (bind_result < 0) {
        this->error_code = GET_NET_SOCKET_ERROR();
        this->error_state = SOCK_BIND;
        
        // Close socket on error
        CLOSE_SOCKET(this->sockfd);
        this->sockfd = INVALID_SOCKET_T;
        this->connected = false;
        
        return false;
    }

    this->connected = true;
    return true;
}

/**
 * Bind to previously set port
 * \return true if successful, false otherwise
 */
bool CServerSocketUDP::Listen() {
    return this->Listen(this->port);
}

}  // namespace net

#endif  // _CSERVER_SOCKET_UDP_CPP