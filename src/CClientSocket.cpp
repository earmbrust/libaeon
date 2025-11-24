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
    // Validate port range
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
 * \return true if connection succeeded, false otherwise
 * 
 * OPTIMIZED FOR MINIMAL LATENCY:
 * - Sets SO_REUSEADDR immediately after socket creation to prevent TIME_WAIT blocks
 * - Sets TCP_NODELAY immediately after socket creation to disable Nagle's algorithm
 * - Reuses sockets when possible to avoid recreation overhead
 * - Minimizes system calls during connection attempts
 * 
 * These optimizations can significantly reduce connection latency:
 * - TIME_WAIT blocking can add 30-120 seconds of delay
 * - Nagle's algorithm can add 40ms+ latency per message
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

    // Track address family of current socket to avoid unnecessary recreation
    int current_family = this->net_family;

    // Try each address until we succeed
    for (connection = server_info; connection != nullptr; connection = connection->ai_next) {
        socket_t sock = INVALID_SOCKET_T;
        
        // Only create a new socket if we're changing address families
        // This avoids unnecessary socket recreation overhead when retrying within same family
        if (connection->ai_family != current_family) {
            // Different family - need new socket
            sock = socket(connection->ai_family, connection->ai_socktype,
                         connection->ai_protocol);
            
            if (!IsValidSocket(sock)) {
                continue;  // Try next address
            }
            
            // CRITICAL: Apply socket configuration IMMEDIATELY after socket creation
            // This prevents TIME_WAIT delays and disables Nagle's algorithm before any connect attempt
            ConfigureSocketForConnect(sock);
            
            // Close old socket and replace it
            if (IsValidSocket(this->sockfd)) {
                CLOSE_SOCKET(this->sockfd);
            }
            this->sockfd = sock;
            current_family = connection->ai_family;
        }

        // Handle connection with optional timeout
        bool connect_succeeded = false;
        
        if (this->connect_timeout_ms > 0) {
            // Use RAII guard to manage blocking mode - automatically restores on exit
            BlockingModeGuard blocking_guard(this);
            
            if (!blocking_guard.IsValid()) {
                // Failed to set non-blocking mode
                this->error_code = GET_NET_SOCKET_ERROR();
                this->error_state = SOCK_CONNECT;
                CLOSE_SOCKET(this->sockfd);
                this->sockfd = INVALID_SOCKET_T;
                continue;  // Try next address
            }
            
            int connect_result = connect(this->sockfd, connection->ai_addr, 
                                        GetAddrLen(connection->ai_addrlen));
            
            // On Windows: SOCKET_ERROR with WSAEWOULDBLOCK = connection in progress
            // On POSIX: -1 with EINPROGRESS = connection in progress
            int would_block_error = 0;
#ifdef PLATFORM_WINDOWS
            would_block_error = WSAEWOULDBLOCK;
#else
            would_block_error = EINPROGRESS;
#endif
            
            if (connect_result == 0) {
                // Connected immediately
                connect_succeeded = true;
            } else if (connect_result == NET_SOCKET_ERROR) {
                int err = GET_NET_SOCKET_ERROR();
                if (err == would_block_error) {
                    // Connection in progress - wait with timeout
                    int wait_result = CSocket::WaitForWritable(this->sockfd, this->connect_timeout_ms);
                    
                    if (wait_result > 0) {
                        // Socket is writable - check if connection succeeded
                        int so_error = 0;
                        socklen_t len = sizeof(so_error);
                        int opt_result = getsockopt(this->sockfd, SOL_SOCKET, SO_ERROR, 
                                                    (char*)&so_error, &len);
                        
                        if (opt_result != 0) {
                            // getsockopt() failed
                            this->error_code = GET_NET_SOCKET_ERROR();
                            this->error_state = SOCK_CONNECT;
                        } else if (so_error == 0) {
                            // Connection succeeded
                            connect_succeeded = true;
                        } else {
                            // Connection failed with specific error
                            this->error_code = so_error;
                            this->error_state = SOCK_CONNECT;
                        }
                    } else if (wait_result == 0) {
                        // Timeout
#ifdef PLATFORM_WINDOWS
                        this->error_code = WSAETIMEDOUT;
#else
                        this->error_code = ETIMEDOUT;
#endif
                        this->error_state = SOCK_CONNECT;
                    } else {
                        // select() error
                        this->error_code = GET_NET_SOCKET_ERROR();
                        this->error_state = SOCK_CONNECT;
                    }
                } else {
                    // Immediate connection failure
                    this->error_code = err;
                    this->error_state = SOCK_CONNECT;
                }
            }
            // Guard destructor automatically restores blocking mode here
        } else {
            // Standard blocking connect (no timeout)
            if (connect(this->sockfd, connection->ai_addr, GetAddrLen(connection->ai_addrlen)) == 0) {
                connect_succeeded = true;
            } else {
                this->error_code = GET_NET_SOCKET_ERROR();
                this->error_state = SOCK_CONNECT;
            }
        }
        
        if (connect_succeeded) {
            // Success
            this->connected = true;
            this->net_family = connection->ai_family;
            freeaddrinfo(server_info);
            this->ClearBuffers();
            return true;
        }

        // Connection failed - socket remains for potential retry with different address
        // This avoids recreation if next address is same family (performance optimization)
    }

    // Failed to connect with any address
    freeaddrinfo(server_info);
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