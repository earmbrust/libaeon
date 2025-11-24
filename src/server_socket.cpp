#ifndef _CSERVER_SOCKET_CPP
#define _CSERVER_SOCKET_CPP

#include <net.h>
#include <cstring>
#include <cerrno>
#include <cstdio>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

namespace net {

/**
 * server_socket default constructor
 */
server_socket::server_socket() {
    this->accept_timeout_ms = 0;
}

/**
 * server_socket destructor
 */
server_socket::~server_socket() {
    if (this->is_valid_socket()) {
        NET_CLOSE_SOCKET(this->sockfd);
    }
}

int server_socket::set_accept_timeout(int timeout_ms) {
    if (timeout_ms < 0 || timeout_ms > 60000) {
        this->error_code = err_no_socket;
        return -1;
    }
    this->accept_timeout_ms = timeout_ms;
    this->error_code = err_none;
    return 0;
}

bool server_socket::listen() {
    return this->listen(this->port);
}

bool server_socket::listen(int port) {
    return this->listen("0.0.0.0", port);
}

/**
 * Listen on a specific address and port
 * \param address Address to bind to (e.g., "0.0.0.0", "127.0.0.1", "::", "::1")
 * \param port Port number to listen on
 * \return true if successful, false otherwise
 */
bool server_socket::listen(const char* address, int port) {
    if (!address) {
        this->error_code = err_no_socket;
        this->error_state = state_bind;
        return false;
    }

    if (!is_valid_port(port)) {
        this->error_code = err_no_socket;
        this->error_state = state_bind;
        return false;
    }

    this->port = port;

    // Determine if this is an IPv6 or IPv4 address
    bool is_ipv6 = (std::strchr(address, ':') != nullptr);
    
    sockaddr_storage serv_addr;
    std::memset(&serv_addr, 0, sizeof(serv_addr));

    if (is_ipv6) {
        // IPv6 address
        socket ipv6_sock(socket::family_ipv6, socket::default_type);
        if (!ipv6_sock.is_valid_socket()) {
            this->error_code = err_no_socket;
            this->error_state = state_create;
            return false;
        }

        // Set IPV6_V6ONLY = 1 for IPv6-only operation on all IPv6 addresses
        ipv6_sock.set_ipv6_v6only(true);

        // Set SO_REUSEADDR
        ipv6_sock.set_so_reuseaddr(true);

        // Set up IPv6 address structure
        struct sockaddr_in6 addr6;
        std::memset(&addr6, 0, sizeof(addr6));
        addr6.sin6_family = socket::family_ipv6;
        addr6.sin6_port = htons(static_cast<u_short>(port));

        // Parse IPv6 address
        int inet_pton_result = inet_pton(socket::family_ipv6, address, &addr6.sin6_addr);
        if (inet_pton_result <= 0) {
            std::fprintf(stderr, "Invalid IPv6 address: %s\n", address);
            ipv6_sock.close();
            this->error_code = err_no_socket;
            this->error_state = state_bind;
            return false;
        }

        // Try to bind IPv6 socket
        if (ipv6_sock.bind((struct sockaddr*)&addr6, sizeof(addr6)) == 0) {
            // Bind succeeded, try to listen
            if (ipv6_sock.listen(SOMAXCONN) == 0) {
                // IPv6 listen succeeded - use it
                std::fprintf(stderr, "Server listening on [%s]:%d (IPv6)\n", address, port);
                this->sockfd = ipv6_sock.sockfd;
                this->connected = true;
                return true;
            } else {
                std::fprintf(stderr, "IPv6 listen failed: %d\n", GET_NET_SOCKET_ERROR());
            }
        } else {
            std::fprintf(stderr, "IPv6 bind failed: %d\n", GET_NET_SOCKET_ERROR());
        }
        
        ipv6_sock.close();
        this->error_code = err_no_socket;
        this->error_state = state_bind;
        return false;
    } else {
        // IPv4 address
        socket ipv4_sock(socket::family_ipv4, socket::default_type);
        
        if (!ipv4_sock.is_valid_socket()) {
            this->error_code = err_no_socket;
            this->error_state = state_create;
            return false;
        }

        // Set SO_REUSEADDR
        if (ipv4_sock.set_so_reuseaddr(true) < 0) {
            this->error_code = ipv4_sock.get_error();
            this->error_state = state_accept;
            ipv4_sock.close();
            return false;
        }

        // Set up IPv4 address structure
        struct sockaddr_in addr4;
        std::memset(&addr4, 0, sizeof(addr4));
        addr4.sin_family = socket::family_ipv4;
        addr4.sin_port = htons(static_cast<u_short>(port));

        // Parse IPv4 address
        int inet_pton_result = inet_pton(socket::family_ipv4, address, &addr4.sin_addr);
        if (inet_pton_result <= 0) {
            std::fprintf(stderr, "Invalid IPv4 address: %s\n", address);
            ipv4_sock.close();
            this->error_code = err_no_socket;
            this->error_state = state_bind;
            return false;
        }

        // Bind socket to port
        if (ipv4_sock.bind((struct sockaddr*)&addr4, sizeof(addr4)) < 0) {
            this->error_code = ipv4_sock.get_error();
            this->error_state = state_bind;
            ipv4_sock.close();
            return false;
        }

        // Start listening
        if (ipv4_sock.listen(SOMAXCONN) < 0) {
            this->error_code = ipv4_sock.get_error();
            this->error_state = state_accept;
            ipv4_sock.close();
            return false;
        }

        this->sockfd = ipv4_sock.sockfd;
        this->connected = true;
        std::fprintf(stderr, "Server listening on %s:%d (IPv4)\n", address, port);
        return true;
    }
}

std::unique_ptr<event_socket> server_socket::accept() {
    return this->accept(false);
}

std::unique_ptr<event_socket> server_socket::accept(bool blocking) {
    auto client_socket = std::make_unique<event_socket>();
    event_socket* result = this->accept(client_socket.get(), blocking);
    if (result) {
        client_socket.release();
        return std::unique_ptr<event_socket>(result);
    }
    return nullptr;
}

event_socket* server_socket::accept(event_socket* client_socket, bool blocking) {
    if (!client_socket) {
        return nullptr;
    }
    
    if (!this->is_valid_socket()) {
        return nullptr;
    }

    if (this->accept_timeout_ms > 0) {
        fd_set readset;
        FD_ZERO(&readset);
        FD_SET(this->sockfd, &readset);
        
        timeval tv;
        tv.tv_sec = this->accept_timeout_ms / 1000;
        tv.tv_usec = (this->accept_timeout_ms % 1000) * 1000;
        
        int select_result;
#ifdef NET_PLATFORM_WINDOWS
        select_result = select(0, &readset, nullptr, nullptr, &tv);
#else
        select_result = select(this->sockfd + 1, &readset, nullptr, nullptr, &tv);
#endif
        
        if (select_result < 0) {
            this->error_code = GET_NET_SOCKET_ERROR();
            this->error_state = state_accept;
            return nullptr;
        }
        
        if (select_result == 0) {
            return nullptr;
        }
    }

    struct sockaddr_storage client_addr;
    socklen_t addr_len = sizeof(client_addr);
    
    socket_t client_fd = ::accept(this->sockfd, 
                               (struct sockaddr*)&client_addr, 
                               &addr_len);

#ifdef NET_PLATFORM_WINDOWS
    bool fd_valid = (client_fd != INVALID_SOCKET);
#else
    bool fd_valid = (client_fd >= 0);
#endif

    if (!fd_valid) {
        int err = GET_NET_SOCKET_ERROR();
        
#ifdef NET_PLATFORM_WINDOWS
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
        this->error_state = state_accept;
        return client_socket;
    }

    if (client_socket->is_valid_socket()) {
        NET_CLOSE_SOCKET(client_socket->sockfd);
    }
    
    client_socket->sockfd = client_fd;
    client_socket->remote_addr = client_addr;
    client_socket->connected = true;
    
    // PERFORMANCE: Apply socket options for low-latency communication
    client_socket->set_socket_tcp_nodelay();          // Disable Nagle's algorithm
    client_socket->set_socket_linger(0);           // Disable linger to avoid TIME_WAIT
    
    client_socket->set_blocking(blocking);

    return client_socket;
}

}  // namespace net

#endif