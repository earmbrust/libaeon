/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#include <net.h>
#include <cstdio>
#include <cstdlib>

namespace net {

/**
 * udp_client default constructor
 */
udp_client_socket::udp_client_socket() {
}

/**
 * udp_client constructor with hostname and port
 * \param hostname Remote hostname to send to
 * \param port Remote port to send to
 */
udp_client_socket::udp_client_socket(const char* hostname, int port) {
    this->connect(hostname, port);
}

/**
 * udp_client constructor with std::string hostname and port
 * \param hostname Remote hostname to send to (as std::string pointer)
 * \param port Remote port to send to
 */
udp_client_socket::udp_client_socket(const std::string* hostname, int port) {
    if (hostname) {
        this->connect(hostname->c_str(), port);
    }
}

/**
 * udp_client destructor
 */
udp_client_socket::~udp_client_socket() {
}

/**
 * Connect with hostname and port parameters
 * \param hostname The remote host to send to
 * \param port The remote port to send to
 * \return true if setup succeeded, false otherwise
 */
bool udp_client_socket::connect(const char* hostname, int port) {
    if (!is_valid_port(port)) {
        this->error_code = err_no_socket;
        this->error_state = state_connect;
        return false;
    }
    this->remote_host = hostname;
    this->port = port;
    
    struct addrinfo hints, *server_info, *connection;
    std::memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    
    int resolve_result = getaddrinfo(this->remote_host.c_str(), 
                                     std::to_string(this->port).c_str(),
                                     &hints, &server_info);
    if (resolve_result != 0) {
        this->error_code = err_no_host;
        this->error_state = state_connect;
        return false;
    }
    
    connection = server_info;
    address resolved_addr;
    
    if (connection->ai_family == AF_INET) {
        resolved_addr = address(*(struct sockaddr_in*)connection->ai_addr);
    } else if (connection->ai_family == AF_INET6) {
        resolved_addr = address(*(struct sockaddr_in6*)connection->ai_addr);
    } else {
        freeaddrinfo(server_info);
        this->error_code = err_no_host;
        this->error_state = state_connect;
        return false;
    }
    
    bool result = this->connect(resolved_addr);
    freeaddrinfo(server_info);
    return result;
}

/**
 * Connect to a specific address
 * \param addr Address object containing the target
 * \return true if setup succeeded, false otherwise
 */
bool udp_client_socket::connect(const address& addr) {
    this->remote_addr = addr.get_sockaddr_storage();
    return this->connect();
}

/**
 * Connect to previously set remote address
 * Creates or recreates socket with correct family and establishes connection
 * \return true if setup succeeded, false otherwise
 */
bool udp_client_socket::connect() {
    if (this->remote_addr.ss_family == 0) {
        this->error_code = err_no_socket;
        this->error_state = state_connect;
        return false;
    }
    
    int target_family = this->remote_addr.ss_family;
    
    // Recreate socket if family doesn't match
    if (target_family != this->net_family) {
        if (this->is_valid_socket()) {
            NET_CLOSE_SOCKET(this->sockfd);
        }
        this->sockfd = ::socket(target_family, socket::datagram_type, 0);
        if (!this->is_valid_socket()) {
            this->error_code = err_no_socket;
            this->error_state = state_create;
            return false;
        }
        this->net_family = target_family;
    }
    
    this->set_socket_reuseaddr();
    this->connected = true;
    return true;
}

}  // namespace net