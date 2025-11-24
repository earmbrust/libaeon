#ifndef _CSERVER_SOCKET_CPP
#define _CSERVER_SOCKET_CPP

#include "libaeon.h"
#include <cstring>
#include <cerrno>
#include <cstdio>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

namespace net {

/**
 * CServerSocket default constructor
 */
CServerSocket::CServerSocket() {
    this->accept_timeout_ms = 0;
}

/**
 * CServerSocket destructor
 */
CServerSocket::~CServerSocket() {
    if (IsValidSocket(this->server_socket)) {
        CLOSE_SOCKET(this->server_socket);
    }
}

int CServerSocket::SetAcceptTimeout(int timeout_ms) {
    if (timeout_ms < 0 || timeout_ms > 60000) {
        this->error_code = ERR_NOSOCKET;
        return -1;
    }
    this->accept_timeout_ms = timeout_ms;
    this->error_code = ERR_NONE;
    return 0;
}

bool CServerSocket::Listen() {
    return this->Listen(this->port);
}

bool CServerSocket::Listen(int port) {
    return this->Listen("0.0.0.0", port);
}

/**
 * Listen on a specific address and port
 * \param address Address to bind to (e.g., "0.0.0.0", "127.0.0.1", "::", "::1")
 * \param port Port number to listen on
 * \return true if successful, false otherwise
 */
bool CServerSocket::Listen(const char* address, int port) {
    if (!address) {
        this->error_code = ERR_NOSOCKET;
        this->error_state = SOCK_BIND;
        return false;
    }

    if (!IsValidPort(port)) {
        this->error_code = ERR_NOSOCKET;
        this->error_state = SOCK_BIND;
        return false;
    }

    this->port = port;

    // Determine if this is an IPv6 or IPv4 address
    bool is_ipv6 = (std::strchr(address, ':') != nullptr);

    if (is_ipv6) {
        // IPv6 address
        socket_t ipv6_sock = socket(AF_INET6, CSocket::DefaultSocketType, 0);
        if (!IsValidSocket(ipv6_sock)) {
            this->error_code = ERR_NOSOCKET;
            this->error_state = SOCK_CREATE;
            return false;
        }

        // Set IPV6_V6ONLY = 1 for IPv6-only operation on all IPv6 addresses
#ifdef IPV6_V6ONLY
        int v6only = 1;
        setsockopt(ipv6_sock, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&v6only, sizeof(v6only));
#endif

        // Set SO_REUSEADDR
        int reuse = 1;
        setsockopt(ipv6_sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));

        // Set up IPv6 address structure
        std::memset(&this->serv_addr, 0, sizeof(this->serv_addr));
        struct sockaddr_in6* addr6 = (struct sockaddr_in6*)&this->serv_addr;
        addr6->sin6_family = AF_INET6;
        addr6->sin6_port = htons(static_cast<u_short>(port));

        // Parse IPv6 address
        int inet_pton_result = inet_pton(AF_INET6, address, &addr6->sin6_addr);
        if (inet_pton_result <= 0) {
            std::fprintf(stderr, "Invalid IPv6 address: %s\n", address);
            CLOSE_SOCKET(ipv6_sock);
            this->error_code = ERR_NOSOCKET;
            this->error_state = SOCK_BIND;
            return false;
        }

        // Try to bind IPv6 socket
        int bind_result = bind(ipv6_sock, 
                              (struct sockaddr*)&this->serv_addr, 
                              sizeof(struct sockaddr_in6));
        
        if (bind_result == 0) {
            // Bind succeeded, try to listen
            int listen_result = listen(ipv6_sock, SOMAXCONN);
            if (listen_result == 0) {
                // IPv6 listen succeeded - use it
                std::fprintf(stderr, "Server listening on [%s]:%d (IPv6)\n", address, port);
                this->server_socket = ipv6_sock;
                return true;
            } else {
                std::fprintf(stderr, "IPv6 listen failed: %d\n", GET_NET_SOCKET_ERROR());
            }
        } else {
            std::fprintf(stderr, "IPv6 bind failed: %d\n", GET_NET_SOCKET_ERROR());
        }
        
        CLOSE_SOCKET(ipv6_sock);
        this->error_code = ERR_NOSOCKET;
        this->error_state = SOCK_BIND;
        return false;
    } else {
        // IPv4 address
        this->server_socket = socket(AF_INET, CSocket::DefaultSocketType, 0);
        
        if (!IsValidSocket(this->server_socket)) {
            this->error_code = ERR_NOSOCKET;
            this->error_state = SOCK_CREATE;
            return false;
        }

        // Set SO_REUSEADDR
        int reuse = 1;
        if (setsockopt(this->server_socket, SOL_SOCKET, SO_REUSEADDR, 
                       (const char*)&reuse, sizeof(reuse)) < 0) {
            this->error_code = GET_NET_SOCKET_ERROR();
            this->error_state = SOCK_ACCEPT;
            CLOSE_SOCKET(this->server_socket);
            return false;
        }

        // Set up IPv4 address structure
        std::memset(&this->serv_addr, 0, sizeof(this->serv_addr));
        struct sockaddr_in* addr4 = (struct sockaddr_in*)&this->serv_addr;
        addr4->sin_family = AF_INET;
        addr4->sin_port = htons(static_cast<u_short>(port));

        // Parse IPv4 address
        int inet_pton_result = inet_pton(AF_INET, address, &addr4->sin_addr);
        if (inet_pton_result <= 0) {
            std::fprintf(stderr, "Invalid IPv4 address: %s\n", address);
            CLOSE_SOCKET(this->server_socket);
            this->error_code = ERR_NOSOCKET;
            this->error_state = SOCK_BIND;
            return false;
        }

        // Bind socket to port
        int bind_result = bind(this->server_socket, 
                              (struct sockaddr*)&this->serv_addr, 
                              sizeof(struct sockaddr_in));
        
        if (bind_result < 0) {
            this->error_code = GET_NET_SOCKET_ERROR();
            this->error_state = SOCK_BIND;
            CLOSE_SOCKET(this->server_socket);
            return false;
        }

        // Start listening
        int listen_result = listen(this->server_socket, SOMAXCONN);
        
        if (listen_result < 0) {
            this->error_code = GET_NET_SOCKET_ERROR();
            this->error_state = SOCK_ACCEPT;
            CLOSE_SOCKET(this->server_socket);
            return false;
        }

        std::fprintf(stderr, "Server listening on %s:%d (IPv4)\n", address, port);
        return true;
    }
}

std::unique_ptr<CEventSocket> CServerSocket::Accept() {
    return this->Accept(false);
}

std::unique_ptr<CEventSocket> CServerSocket::Accept(bool blocking) {
    auto client_socket = std::make_unique<CEventSocket>();
    CEventSocket* result = this->Accept(client_socket.get(), blocking);
    if (result) {
        client_socket.release();
        return std::unique_ptr<CEventSocket>(result);
    }
    return nullptr;
}

CEventSocket* CServerSocket::Accept(CEventSocket* client_socket, bool blocking) {
    if (!client_socket) {
        return nullptr;
    }
    
    if (!IsValidSocket(this->server_socket)) {
        return nullptr;
    }

    if (this->accept_timeout_ms > 0) {
        fd_set readset;
        FD_ZERO(&readset);
        FD_SET(this->server_socket, &readset);
        
        timeval tv;
        tv.tv_sec = this->accept_timeout_ms / 1000;
        tv.tv_usec = (this->accept_timeout_ms % 1000) * 1000;
        
        int select_result;
#ifdef PLATFORM_WINDOWS
        select_result = select(0, &readset, nullptr, nullptr, &tv);
#else
        select_result = select(this->server_socket + 1, &readset, nullptr, nullptr, &tv);
#endif
        
        if (select_result < 0) {
            this->error_code = GET_NET_SOCKET_ERROR();
            this->error_state = SOCK_ACCEPT;
            return nullptr;
        }
        
        if (select_result == 0) {
            return nullptr;
        }
    }

    struct sockaddr_storage client_addr;
    socklen_t addr_len = sizeof(client_addr);
    
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
        
        client_socket->connected = false;
        this->error_code = err;
        this->error_state = SOCK_ACCEPT;
        return client_socket;
    }

    if (IsValidSocket(client_socket->sockfd)) {
        CLOSE_SOCKET(client_socket->sockfd);
    }
    
    client_socket->sockfd = client_fd;
    client_socket->remote_addr = client_addr;
    client_socket->connected = true;
    
    // PERFORMANCE: Apply socket options for low-latency communication
    SetSocketTCPNodelay(client_fd);          // Disable Nagle's algorithm
    SetSocketLinger(client_fd, 0);           // Disable linger to avoid TIME_WAIT
    
    client_socket->SetBlocking(blocking);

    return client_socket;
}

}  // namespace net

#endif