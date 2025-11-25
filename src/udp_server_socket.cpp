/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#include <net.h>
#include <cstring>

namespace net {

/**
 * Bind to specified port for receiving UDP datagrams
 * \param port Port number to bind to
 * \return true if successful, false otherwise
 */
bool udp_server_socket::listen(int port) {
    this->port = port;

    // UDP socket already created in udp_socket constructor (as IPv4)
    if (!this->is_valid_socket()) {
        this->error_code = err_no_socket;
        this->error_state = state_create;
        return false;
    }

    // Validate port range
    if (!is_valid_port(port)) {
        this->error_code = err_no_socket;
        this->error_state = state_bind;
        return false;
    }

    // Try IPv6 first with dual-stack mode (can accept both IPv4 and IPv6)
    // If that fails, fall back to IPv4
    
    // Try to create and bind IPv6 socket first
    socket_t ipv6_sock = ::socket(AF_INET6, socket::datagram_type, 0);
#ifdef NET_PLATFORM_WINDOWS
    bool ipv6_valid = (ipv6_sock != INVALID_SOCKET);
#else
    bool ipv6_valid = (ipv6_sock >= 0);
#endif

    if (ipv6_valid) {
        // Try to enable dual-stack mode (accepts both IPv4 and IPv6)
        // This is not available on all platforms, so we ignore failures
#ifdef IPV6_V6ONLY
        int v6only = 0;
        setsockopt(ipv6_sock, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&v6only, sizeof(v6only));
#endif

        // Set up IPv6 address structure (local variable)
        sockaddr_storage serv_addr;
        std::memset(&serv_addr, 0, sizeof(serv_addr));
        struct sockaddr_in6* addr6 = (struct sockaddr_in6*)&serv_addr;
        addr6->sin6_family = AF_INET6;
        addr6->sin6_addr = in6addr_any;  // Listen on any interface
        addr6->sin6_port = htons(static_cast<u_short>(port));

        // Try to bind IPv6 socket
        int bind_result = ::bind(ipv6_sock, 
                              (struct sockaddr*)&serv_addr, 
                              sizeof(struct sockaddr_in6));
        
        if (bind_result == 0) {
            // IPv6 bind succeeded - close old IPv4 socket and use IPv6
            NET_CLOSE_SOCKET(this->sockfd);
            this->sockfd = ipv6_sock;
            this->listening = true;
            return true;
        } else {
            // IPv6 bind failed, close it and try IPv4
            NET_CLOSE_SOCKET(ipv6_sock);
        }
    }

    // Fall back to IPv4
    // Set up IPv4 address structure (local variable)
    sockaddr_storage serv_addr;
    std::memset(&serv_addr, 0, sizeof(serv_addr));
    struct sockaddr_in* addr4 = (struct sockaddr_in*)&serv_addr;
    addr4->sin_family = AF_INET;
    addr4->sin_addr.s_addr = htonl(INADDR_ANY);  // Listen on any interface
    addr4->sin_port = htons(static_cast<u_short>(port));

    // Bind IPv4 socket
    int bind_result = ::bind(this->sockfd, 
                          (struct sockaddr*)&serv_addr, 
                          sizeof(struct sockaddr_in));
    
    if (bind_result < 0) {
        this->error_code = GET_NET_SOCKET_ERROR();
        this->error_state = state_bind;
        
        // Close socket on error
        NET_CLOSE_SOCKET(this->sockfd);
        this->sockfd = invalid_socket;
        this->listening = false;
        
        return false;
    }

    this->listening = true;
    return true;
}

/**
 * Bind to previously set port
 * \return true if successful, false otherwise
 */
bool udp_server_socket::listen() {
    return this->listen(this->port);
}

}  // namespace net