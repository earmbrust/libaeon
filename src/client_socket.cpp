/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#ifndef _CCLIENT_SOCKET_CPP
#define _CCLIENT_SOCKET_CPP

#include <net.h>
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
 * Connect with hostname and port parameters
 * \param hostname The remote host to connect to
 * \param remote_port The remote port to connect to
 * \return true if connection succeeded, false otherwise
 */
bool client_socket::connect(const char* hostname, int remote_port) {
    if (!is_valid_port(remote_port)) {
        this->error_code = err_no_socket;
        this->error_state = state_connect;
        return false;
    }
    this->remote_host = hostname;
    this->port = remote_port;
    return this->connect();
}

/**
 * Connect to previously set remote host and port
 * Resolves hostname and delegates to Connect(const address&)
 * \return true if connection succeeded, false otherwise
 */
bool client_socket::connect() {
    struct addrinfo hints, *server_info, *connection;
    std::memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (!this->is_valid_socket()) {
        this->error_code = err_no_socket;
        this->error_state = state_create;
        return false;
    }

    std::string port_str = std::to_string(this->port);
    int rv = getaddrinfo(this->remote_host.c_str(), port_str.c_str(), &hints, &server_info);
    if (rv != 0) {
        this->error_code = err_no_host;
        this->error_state = state_resolve;
        return false;
    }

    // Try each address until we succeed
    for (connection = server_info; connection != nullptr; connection = connection->ai_next) {
        address addr;
        if (connection->ai_family == AF_INET) {
            addr = address(*(sockaddr_in*)connection->ai_addr);
        } else if (connection->ai_family == AF_INET6) {
            addr = address(*(sockaddr_in6*)connection->ai_addr);
        } else {
            continue;
        }

        if (this->connect(addr)) {
            freeaddrinfo(server_info);
            return true;
        }
    }

    freeaddrinfo(server_info);
    this->connected = false;
    NET_CLOSE_SOCKET(this->sockfd);
    this->sockfd = invalid_socket;
    return false;
}

/**
 * Connect to a previously resolved address
 * \param addr address object containing the remote address and port
 * \return true if connection succeeded, false otherwise
 * 
 * Single source of truth for actual connection logic.
 * Handles socket creation, family changes, timeouts, and error handling.
 */
bool client_socket::connect(const address& addr) {
    sockaddr_storage target_addr = addr.get_sockaddr_storage();
    int target_family = addr.is_ipv6() ? AF_INET6 : AF_INET;
    socklen_t addr_len = addr.is_ipv6() ? sizeof(sockaddr_in6) : sizeof(sockaddr_in);
    
    // Create new socket if family doesn't match
    if (target_family != this->net_family) {
        if (this->is_valid_socket()) {
            NET_CLOSE_SOCKET(this->sockfd);
        }
        this->sockfd = ::socket(target_family, socket::stream_type, 0);
        
        if (!this->is_valid_socket()) {
            this->error_code = err_no_socket;
            this->error_state = state_create;
            return false;
        }
        
        this->configure_socket_for_connect();
        this->net_family = target_family;
    }
    
    // Handle connection with optional timeout
    bool connect_succeeded = false;
    
    if (this->connect_timeout_ms > 0) {
        blocking_mode_guard blocking_guard(this);
        
        if (!blocking_guard.is_valid()) {
            this->error_code = GET_NET_SOCKET_ERROR();
            this->error_state = state_connect;
            NET_CLOSE_SOCKET(this->sockfd);
            this->sockfd = invalid_socket;
            return false;
        }
        
        int connect_result = ::connect(this->sockfd, (struct sockaddr*)&target_addr, addr_len);
        
        int would_block_error = 0;
#ifdef NET_PLATFORM_WINDOWS
        would_block_error = WSAEWOULDBLOCK;
#else
        would_block_error = EINPROGRESS;
#endif
        
        if (connect_result == 0) {
            connect_succeeded = true;
        } else if (connect_result == NET_SOCKET_ERROR) {
            int err = GET_NET_SOCKET_ERROR();
            if (err == would_block_error) {
                int wait_result = socket::wait_for_writable(this->sockfd, this->connect_timeout_ms);
                
                if (wait_result > 0) {
                    int so_error = 0;
                    socklen_t len = sizeof(so_error);
                    int opt_result = getsockopt(this->sockfd, SOL_SOCKET, SO_ERROR, 
                                                (char*)&so_error, &len);
                    
                    if (opt_result != 0) {
                        this->error_code = GET_NET_SOCKET_ERROR();
                        this->error_state = state_connect;
                    } else if (so_error == 0) {
                        connect_succeeded = true;
                    } else {
                        this->error_code = so_error;
                        this->error_state = state_connect;
                    }
                } else if (wait_result == 0) {
#ifdef NET_PLATFORM_WINDOWS
                    this->error_code = WSAETIMEDOUT;
#else
                    this->error_code = ETIMEDOUT;
#endif
                    this->error_state = state_connect;
                } else {
                    this->error_code = GET_NET_SOCKET_ERROR();
                    this->error_state = state_connect;
                }
            } else {
                this->error_code = err;
                this->error_state = state_connect;
            }
        }
    } else {
        // Standard blocking connect
        if (::connect(this->sockfd, (struct sockaddr*)&target_addr, addr_len) == 0) {
            connect_succeeded = true;
        } else {
            this->error_code = GET_NET_SOCKET_ERROR();
            this->error_state = state_connect;
        }
    }
    
    if (connect_succeeded) {
        this->connected = true;
        this->clear_buffers();
        return true;
    }
    
    this->connected = false;
    NET_CLOSE_SOCKET(this->sockfd);
    this->sockfd = invalid_socket;
    return false;
}

/**
 * client_socket default constructor
 */
client_socket::client_socket() {
    this->sockfd = ::socket(socket::default_family, socket::stream_type, 0);
}

/**
 * client_socket constructor with hostname and port
 * \param hostname Remote hostname to connect to
 * \param port Remote port to connect to
 */
client_socket::client_socket(const char* hostname, int port) {
    this->sockfd = ::socket(socket::default_family, socket::stream_type, 0);
    this->connect(hostname, port);
}

/**
 * client_socket constructor with std::string hostname and port
 * \param hostname Remote hostname to connect to (as std::string pointer)
 * \param port Remote port to connect to
 */
client_socket::client_socket(const std::string* hostname, int port) {
    this->sockfd = ::socket(socket::default_family, socket::stream_type, 0);
    if (hostname) {
        this->connect(hostname->c_str(), port);
    }
}

/**
 * client_socket destructor
 */
client_socket::~client_socket() {
}

}  // namespace net

#endif  // _CCLIENT_SOCKET_CPP