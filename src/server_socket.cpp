#ifndef _CSERVER_SOCKET_CPP
#define _CSERVER_SOCKET_CPP

#include <aeon.hpp>
#include <cstring>
#include <cerrno>
#include <cstdio>

#ifdef HAVE_CONFIG_H
#include "../config.hpp"
#endif

namespace aeon {

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
 * Bind socket to address
 * \param addr Pointer to sockaddr structure
 * \param addrlen Size of address structure
 * \return 0 on success, -1 on error
 */
int server_socket::bind(const sockaddr* addr, socklen_t addrlen) {
    if (!this->is_valid_socket()) {
        this->error_code = err_no_socket;
        this->error_state = state_create;
        return -1;
    }

    if (!addr) {
        this->error_code = err_no_socket;
        this->error_state = state_bind;
        return -1;
    }

    if (::bind(this->sockfd, addr, addrlen) < 0) {
        this->error_code = GET_NET_SOCKET_ERROR();
        this->error_state = state_bind;
        return -1;
    }

    this->error_code = err_none;
    return 0;
}


/**
 * Listen on a specific address and port
 * Follows the pattern from client_socket::connect() - manages this->sockfd directly
 * instead of creating temporary socket objects
 * 
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

    // Determine address family by attempting to parse as IPv6 first, then IPv4
    struct sockaddr_in6 addr6;
    struct sockaddr_in addr4;
    int target_family = AF_UNSPEC;
    sockaddr* bind_addr = nullptr;
    socklen_t bind_addr_len = 0;

    std::memset(&addr6, 0, sizeof(addr6));
    std::memset(&addr4, 0, sizeof(addr4));

    // Try IPv6 first
    int inet_pton_result = inet_pton(AF_INET6, address, &addr6.sin6_addr);
    if (inet_pton_result > 0) {
        // Valid IPv6 address
        target_family = AF_INET6;
        addr6.sin6_family = AF_INET6;
        addr6.sin6_port = htons(static_cast<u_short>(port));
        bind_addr = (sockaddr*)&addr6;
        bind_addr_len = sizeof(addr6);
    } else {
        // Try IPv4
        inet_pton_result = inet_pton(AF_INET, address, &addr4.sin_addr);
        if (inet_pton_result > 0) {
            // Valid IPv4 address
            target_family = AF_INET;
            addr4.sin_family = AF_INET;
            addr4.sin_port = htons(static_cast<u_short>(port));
            bind_addr = (sockaddr*)&addr4;
            bind_addr_len = sizeof(addr4);
        } else {
            // Invalid address
            std::fprintf(stderr, "Invalid address: %s\n", address);
            this->error_code = err_no_socket;
            this->error_state = state_bind;
            return false;
        }
    }

    // Recreate socket if family doesn't match (similar to client_socket::connect pattern)
    if (target_family != this->net_family) {
        if (this->is_valid_socket()) {
            NET_CLOSE_SOCKET(this->sockfd);
        }

        this->sockfd = ::socket(target_family, socket::default_type, 0);
        if (!this->is_valid_socket()) {
            this->error_code = err_no_socket;
            this->error_state = state_create;
            return false;
        }

        this->net_family = target_family;
    }

    // Set socket options
    this->set_so_reuseaddr(true);

    if (target_family == AF_INET6) {
        // For IPv6, set IPV6_V6ONLY = 1 for IPv6-only operation
        this->set_ipv6_v6only(true);
    }

    // Bind to address and port
    int bind_result = this->bind(bind_addr, bind_addr_len);
    if (bind_result < 0) {
        this->error_code = GET_NET_SOCKET_ERROR();
        this->error_state = state_bind;
        NET_CLOSE_SOCKET(this->sockfd);
        this->sockfd = invalid_socket;
        return false;
    }

    // Start listening
    int listen_result = ::listen(this->sockfd, SOMAXCONN);
    if (listen_result < 0) {
        this->error_code = GET_NET_SOCKET_ERROR();
        this->error_state = state_accept;
        NET_CLOSE_SOCKET(this->sockfd);
        this->sockfd = invalid_socket;
        return false;
    }

    this->connected = true;
    const char* family_str = (target_family == AF_INET6) ? "IPv6" : "IPv4";
    const char* bracket_open = (target_family == AF_INET6) ? "[" : "";
    const char* bracket_close = (target_family == AF_INET6) ? "]" : "";
    std::fprintf(stderr, "Server listening on %s%s%s:%d (%s)\n", 
                 bracket_open, address, bracket_close, port, family_str);

    return true;
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
    client_socket->set_socket_linger(0);             // Disable linger to avoid TIME_WAIT
    
    client_socket->set_blocking(blocking);

    return client_socket;
}

}  // aeon

#endif