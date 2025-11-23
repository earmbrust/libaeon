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

// Platform-specific helper macros
#ifdef PLATFORM_WINDOWS
    #define CLOSE_SOCKET(s) closesocket(s)
    #define GET_NET_SOCKET_ERROR() WSAGetLastError()
#else
    #define CLOSE_SOCKET(s) close(s)
    #define GET_NET_SOCKET_ERROR() errno
#endif

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

    // UDP socket already created in CSocketUDP constructor
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

    // Set up server address structure
    std::memset(&this->serv_addr, 0, sizeof(this->serv_addr));
    this->serv_addr.sin_family = CSocket::DefaultFamilyType;
    this->serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);  // Listen on any interface
    this->serv_addr.sin_port = htons(static_cast<u_short>(port));

    // Bind socket to port
    int bind_result = bind(this->sockfd, 
                          (struct sockaddr*)&this->serv_addr, 
                          sizeof(this->serv_addr));
    
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