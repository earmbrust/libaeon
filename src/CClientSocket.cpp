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
#include <cerrno>

namespace net {


// Helper: Convert addrinfo.ai_addrlen (size_t on Windows, socklen_t on POSIX) to socklen_t
// On Windows, ai_addrlen is size_t; on POSIX it's socklen_t. This handles both.
static inline socklen_t GetAddrLen(size_t len) {
    // Safe: address lengths for IPv4/IPv6 are always < 256 bytes
    return static_cast<socklen_t>(len);
}

/**
 * Helper: Configure socket for optimal connection performance
 * Centralizes SO_REUSEADDR and TCP_NODELAY configuration
 * 
 * PERFORMANCE IMPACT:
 * - SO_REUSEADDR: Prevents 30-120 second TIME_WAIT delays on reconnection
 * - TCP_NODELAY: Eliminates 40ms+ latency per message from Nagle's algorithm
 */
static inline void ConfigureSocketForConnect(socket_t sock) {
    SetSocketReusAddr(sock);
    SetSocketTCPNodelay(sock);
}

/**
 * Connect with hostname and port parameters
 * \param hostname The remote host to connect to
 * \param remote_port The remote port to connect to
 * \return true if connection succeeded, false otherwise
 */
bool CClientSocket::Connect(const char* hostname, int remote_port) {
    if (!IsValidPort(remote_port)) {
        this->error_code = ERR_NOSOCKET;
        this->error_state = SOCK_CONNECT;
        return false;
    }
    this->remote_host = hostname;
    this->port = remote_port;
    return this->Connect();
}

/**
 * Connect to previously set remote host and port
 * Resolves hostname and delegates to Connect(const CAddress&)
 * \return true if connection succeeded, false otherwise
 */
bool CClientSocket::Connect() {
    struct addrinfo hints, *server_info, *connection;
    std::memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

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

    // Try each address until we succeed
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
    CLOSE_SOCKET(this->sockfd);
    this->sockfd = INVALID_SOCKET_T;
    return false;
}

/**
 * Connect to a previously resolved CAddress
 * \param addr CAddress object containing the remote address and port
 * \return true if connection succeeded, false otherwise
 * 
 * Single source of truth for actual connection logic.
 * Handles socket creation, family changes, timeouts, and error handling.
 */
bool CClientSocket::Connect(const CAddress& addr) {
    sockaddr_storage target_addr = addr.GetSockaddrStorage();
    int target_family = addr.IsIPv6() ? AF_INET6 : AF_INET;
    socklen_t addr_len = addr.IsIPv6() ? sizeof(sockaddr_in6) : sizeof(sockaddr_in);
    
    // Create new socket if family doesn't match
    if (target_family != this->net_family) {
        if (IsValidSocket(this->sockfd)) {
            CLOSE_SOCKET(this->sockfd);
        }
        this->sockfd = socket(target_family, CSocket::StreamSocketType, 0);
        
        if (!IsValidSocket(this->sockfd)) {
            this->error_code = ERR_NOSOCKET;
            this->error_state = SOCK_CREATE;
            return false;
        }
        
        ConfigureSocketForConnect(this->sockfd);
        this->net_family = target_family;
    }
    
    // Handle connection with optional timeout
    bool connect_succeeded = false;
    
    if (this->connect_timeout_ms > 0) {
        BlockingModeGuard blocking_guard(this);
        
        if (!blocking_guard.IsValid()) {
            this->error_code = GET_NET_SOCKET_ERROR();
            this->error_state = SOCK_CONNECT;
            CLOSE_SOCKET(this->sockfd);
            this->sockfd = INVALID_SOCKET_T;
            return false;
        }
        
        int connect_result = connect(this->sockfd, (struct sockaddr*)&target_addr, addr_len);
        
        int would_block_error = 0;
#ifdef PLATFORM_WINDOWS
        would_block_error = WSAEWOULDBLOCK;
#else
        would_block_error = EINPROGRESS;
#endif
        
        if (connect_result == 0) {
            connect_succeeded = true;
        } else if (connect_result == NET_SOCKET_ERROR) {
            int err = GET_NET_SOCKET_ERROR();
            if (err == would_block_error) {
                int wait_result = CSocket::WaitForWritable(this->sockfd, this->connect_timeout_ms);
                
                if (wait_result > 0) {
                    int so_error = 0;
                    socklen_t len = sizeof(so_error);
                    int opt_result = getsockopt(this->sockfd, SOL_SOCKET, SO_ERROR, 
                                                (char*)&so_error, &len);
                    
                    if (opt_result != 0) {
                        this->error_code = GET_NET_SOCKET_ERROR();
                        this->error_state = SOCK_CONNECT;
                    } else if (so_error == 0) {
                        connect_succeeded = true;
                    } else {
                        this->error_code = so_error;
                        this->error_state = SOCK_CONNECT;
                    }
                } else if (wait_result == 0) {
#ifdef PLATFORM_WINDOWS
                    this->error_code = WSAETIMEDOUT;
#else
                    this->error_code = ETIMEDOUT;
#endif
                    this->error_state = SOCK_CONNECT;
                } else {
                    this->error_code = GET_NET_SOCKET_ERROR();
                    this->error_state = SOCK_CONNECT;
                }
            } else {
                this->error_code = err;
                this->error_state = SOCK_CONNECT;
            }
        }
    } else {
        // Standard blocking connect
        if (connect(this->sockfd, (struct sockaddr*)&target_addr, addr_len) == 0) {
            connect_succeeded = true;
        } else {
            this->error_code = GET_NET_SOCKET_ERROR();
            this->error_state = SOCK_CONNECT;
        }
    }
    
    if (connect_succeeded) {
        this->connected = true;
        this->ClearBuffers();
        return true;
    }
    
    this->connected = false;
    CLOSE_SOCKET(this->sockfd);
    this->sockfd = INVALID_SOCKET_T;
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