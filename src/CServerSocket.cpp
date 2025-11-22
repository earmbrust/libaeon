/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#ifndef _CSERVER_SOCKET_CPP
#define _CSERVER_SOCKET_CPP

#include "libaeon.h"
#include <cstring>

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
 * \return Pointer to new CSocket with client connection, or nullptr on error
 * \note Caller is responsible for deleting the returned CSocket
 */
CSocket* CServerSocket::Accept() {
    if (!IsValidSocket(this->server_socket)) {
        return nullptr;
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
        this->error_code = GET_SOCKET_ERROR();
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