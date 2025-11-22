/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2018 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 * 
 * Modernized for cross-platform socket_t handling
 *********************************************************************/

#ifndef _CSERVER_SOCKET_CPP
#define _CSERVER_SOCKET_CPP

#include "libaeon.h"
#include <cstring>
#include <cerrno>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

namespace net {

// Platform-specific helper macros
#ifdef PLATFORM_WINDOWS
    #define CLOSE_SOCKET(s) closesocket(s)
    #define GET_SOCKET_ERROR() WSAGetLastError()
#else
    #define CLOSE_SOCKET(s) close(s)
    #define GET_SOCKET_ERROR() errno
#endif

/**
 * CServerSocket default constructor
 */
CServerSocket::CServerSocket() {
    this->accept_timeout_ms = 0;  // No timeout by default
}

/**
 * CServerSocket destructor
 */
CServerSocket::~CServerSocket() {
    if (IsValidSocket(this->server_socket)) {
        CLOSE_SOCKET(this->server_socket);
    }
}


/**
 * Set the timeout for Accept() calls
 * \param timeout_ms Timeout in milliseconds (0 = no timeout, blocking mode)
 */
void CServerSocket::SetAcceptTimeout(int timeout_ms) {
    this->accept_timeout_ms = timeout_ms;
}
/**
 * Start listening on previously set port
 * \return true if successful, false otherwise
 */
bool CServerSocket::Listen() {
    return this->Listen(this->port);
}

/**
 * Start listening on specified port
 * \param port Port number to listen on
 * \return true if successful, false otherwise
 */
bool CServerSocket::Listen(int port) {
    this->port = port;

    // Create server socket
    this->server_socket = socket(CSocket::DefaultFamilyType, 
                                CSocket::DefaultSocketType, 0);
    
    if (!IsValidSocket(this->server_socket)) {
        this->error_code = ERR_NOSOCKET;
        this->error_state = SOCK_CREATE;
        return false;
    }

    // Set SO_REUSEADDR to allow reusing the socket quickly
    int reuse = 1;
    if (setsockopt(this->server_socket, SOL_SOCKET, SO_REUSEADDR, 
                   (const char*)&reuse, sizeof(reuse)) < 0) {
        this->error_code = GET_SOCKET_ERROR();
        this->error_state = SOCK_ACCEPT;
        CLOSE_SOCKET(this->server_socket);
        return false;
    }

    // Set up server address structure
    std::memset(&this->serv_addr, 0, sizeof(this->serv_addr));
    this->serv_addr.sin_family = CSocket::DefaultFamilyType;
    this->serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);  // Listen on any interface
    this->serv_addr.sin_port = htons(static_cast<u_short>(port));

    // Bind socket to port
    int bind_result = bind(this->server_socket, 
                          (struct sockaddr*)&this->serv_addr, 
                          sizeof(this->serv_addr));
    
    if (bind_result < 0) {
        this->error_code = GET_SOCKET_ERROR();
        this->error_state = SOCK_ACCEPT;
        CLOSE_SOCKET(this->server_socket);
        return false;
    }

    // Start listening for connections (backlog of 5)
    int listen_result = listen(this->server_socket, 5);
    
    if (listen_result < 0) {
        this->error_code = GET_SOCKET_ERROR();
        this->error_state = SOCK_ACCEPT;
        CLOSE_SOCKET(this->server_socket);
        return false;
    }

    return true;
}


/**
 * Accept an incoming client connection
 * \return Pointer to new CSocket with client connection, nullptr if no connection available (non-blocking), or error socket
 * \note Caller is responsible for deleting the returned CSocket
 */
CSocket* CServerSocket::Accept() {
    if (!IsValidSocket(this->server_socket)) {
        return nullptr;
    }

    // If a timeout is set, use select() to wait with timeout
    if (this->accept_timeout_ms > 0) {
#ifdef PLATFORM_WINDOWS
        fd_set readset;
        FD_ZERO(&readset);
        FD_SET(this->server_socket, &readset);
        
        timeval tv;
        tv.tv_sec = this->accept_timeout_ms / 1000;
        tv.tv_usec = (this->accept_timeout_ms % 1000) * 1000;
        
        int select_result = select(0, &readset, nullptr, nullptr, &tv);
        
        if (select_result == SOCKET_ERROR) {
            this->error_code = GET_SOCKET_ERROR();
            this->error_state = SOCK_ACCEPT;
            return nullptr;
        }
        
        if (select_result == 0) {
            // Timeout occurred
            return nullptr;
        }
#else
        fd_set readset;
        FD_ZERO(&readset);
        FD_SET(this->server_socket, &readset);
        
        timeval tv;
        tv.tv_sec = this->accept_timeout_ms / 1000;
        tv.tv_usec = (this->accept_timeout_ms % 1000) * 1000;
        
        int select_result = select(this->server_socket + 1, &readset, nullptr, nullptr, &tv);
        
        if (select_result < 0) {
            this->error_code = GET_SOCKET_ERROR();
            this->error_state = SOCK_ACCEPT;
            return nullptr;
        }
        
        if (select_result == 0) {
            // Timeout occurred
            return nullptr;
        }
#endif
    }

    CSocket* client_socket = new CSocket();
    if (!client_socket) {
        return nullptr;
    }

    // Accept connection from client
    socklen_t addr_len = sizeof(client_socket->remote_addr);
    socket_t client_fd = accept(this->server_socket, 
                               (struct sockaddr*)&client_socket->remote_addr, 
                               &addr_len);

    if (!IsValidSocket(client_fd)) {
        int err = GET_SOCKET_ERROR();
        
        // Check if this is a non-blocking socket with no pending connections
#ifdef PLATFORM_WINDOWS
        if (err == WSAEWOULDBLOCK || err == WSAECONNRESET) {
            // Non-blocking and no connection available - not an error
            delete client_socket;
            return nullptr;
        }
#else
        if (err == EAGAIN || err == EWOULDBLOCK || err == ECONNRESET) {
            // Non-blocking and no connection available - not an error
            delete client_socket;
            return nullptr;
        }
#endif
        
        // Actual error
        this->error_code = err;
        this->error_state = SOCK_ACCEPT;
        client_socket->connected = false;
        return client_socket;
    }

    // Set up the client socket
    client_socket->sockfd = client_fd;
    client_socket->connected = true;

    return client_socket;
}

}  // namespace net

#endif  // _CSERVER_SOCKET_CPP