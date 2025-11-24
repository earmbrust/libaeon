/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
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
 * \return 0 on success, -1 on error (check GetError() for details)
 * 
 * When a timeout is set, Accept() will return immediately if no pending
 * connections are available, after waiting the specified time.
 * Use 0 for no timeout (blocking mode, wait indefinitely).
 * 
 * Error code set on failure:
 * - ERR_NOSOCKET: Invalid timeout value (negative)
 */
int CServerSocket::SetAcceptTimeout(int timeout_ms) {
    if (timeout_ms < 0 || timeout_ms > 60000) {  // Cap at 60 seconds as per audit
        this->error_code = ERR_NOSOCKET;
        return -1;
    }
    this->accept_timeout_ms = timeout_ms;
    this->error_code = ERR_NONE;  // Clear previous errors on success
    return 0;
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
    // Validate port range
    if (!IsValidPort(port)) {
        this->error_code = ERR_NOSOCKET;
        this->error_state = SOCK_BIND;
        return false;
    }

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
        this->error_code = GET_NET_SOCKET_ERROR();
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
        this->error_code = GET_NET_SOCKET_ERROR();
        this->error_state = SOCK_BIND;
        CLOSE_SOCKET(this->server_socket);
        return false;
    }

    // Start listening for connections (backlog of 5)
    int listen_result = listen(this->server_socket, 5);
    
    if (listen_result < 0) {
        this->error_code = GET_NET_SOCKET_ERROR();
        this->error_state = SOCK_ACCEPT;
        CLOSE_SOCKET(this->server_socket);
        return false;
    }

    return true;
}


/**
 * Accept an incoming client connection
 * \return unique_ptr to new CEventSocket with client connection, non-blocking by default
 */
std::unique_ptr<CEventSocket> CServerSocket::Accept() {
    return this->Accept(false);  // Non-blocking by default for event-based
}

/**
 * Accept an incoming client connection with specified blocking mode
 * \param blocking true for blocking, false for non-blocking
 * \return unique_ptr to new CEventSocket with client connection, nullptr if no connection available (non-blocking), or error socket
 */
std::unique_ptr<CEventSocket> CServerSocket::Accept(bool blocking) {
    auto client_socket = std::make_unique<CEventSocket>();
    CEventSocket* result = this->Accept(client_socket.get(), blocking);
    if (result) {
        // Transfer ownership: we allocated it, now return it via unique_ptr
        client_socket.release();
        return std::unique_ptr<CEventSocket>(result);
    }
    return nullptr;  // client_socket auto-deletes on scope exit
}

/**
 * Accept an incoming client connection into provided socket
 * \param client_socket Existing CEventSocket to populate with connection
 * \param blocking true for blocking, false for non-blocking
 * \return Pointer to populated CEventSocket, nullptr on timeout/no pending, or error socket
 * \note Caller retains responsibility for deleting client_socket
 */
CEventSocket* CServerSocket::Accept(CEventSocket* client_socket, bool blocking) {
    if (!client_socket) {
        return nullptr;
    }
    
    if (!IsValidSocket(this->server_socket)) {
        return nullptr;
    }

    // If a timeout is set, use select() to wait with timeout
    if (this->accept_timeout_ms > 0) {
        fd_set readset;
        FD_ZERO(&readset);
        FD_SET(this->server_socket, &readset);
        
        timeval tv;
        tv.tv_sec = this->accept_timeout_ms / 1000;
        tv.tv_usec = (this->accept_timeout_ms % 1000) * 1000;
        
        // Platform-specific select() call
        // Windows: first param (nfds) is ignored, use 0
        // POSIX: first param must be max fd + 1
        int select_result;
#ifdef PLATFORM_WINDOWS
        select_result = select(0, &readset, nullptr, nullptr, &tv);
#else
        select_result = select(this->server_socket + 1, &readset, nullptr, nullptr, &tv);
#endif
        
        // Unified error checking: select_result < 0 means error on all platforms
        if (select_result < 0) {
            this->error_code = GET_NET_SOCKET_ERROR();
            this->error_state = SOCK_ACCEPT;
            return nullptr;
        }
        
        // select_result == 0 means timeout
        if (select_result == 0) {
            return nullptr;
        }
    }

    // Declare address on stack
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    
    // Accept connection
    socket_t client_fd = accept(this->server_socket, 
                               (struct sockaddr*)&client_addr, 
                               &addr_len);

    if (!IsValidSocket(client_fd)) {
        int err = GET_NET_SOCKET_ERROR();
        
#ifdef PLATFORM_WINDOWS
        if (err == WSAEWOULDBLOCK || err == WSAECONNRESET) {
            return nullptr;
        }
#else
        if (err == EAGAIN || err == EWOULDBLOCK || err == ECONNRESET) {
            return nullptr;
        }
#endif
        
        // Actual error
        client_socket->connected = false;
        this->error_code = err;
        this->error_state = SOCK_ACCEPT;
        return client_socket;
    }

    // Populate the provided socket with the accepted connection
    // First, close any existing socket to prevent leaks
    if (IsValidSocket(client_socket->sockfd)) {
        CLOSE_SOCKET(client_socket->sockfd);
    }
    
    client_socket->sockfd = client_fd;
    client_socket->remote_addr = client_addr;
    client_socket->connected = true;
    
    // Set blocking mode
    client_socket->SetBlocking(blocking);

    return client_socket;
}

}  // namespace net

#endif  // _CSERVER_SOCKET_CPP