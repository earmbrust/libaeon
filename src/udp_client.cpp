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
udp_client::udp_client() {
}

/**
 * udp_client constructor with hostname and port
 * \param hostname Remote hostname to send to
 * \param port Remote port to send to
 */
udp_client::udp_client(const char* hostname, int port) {
    this->connect(hostname, port);
}

/**
 * udp_client constructor with std::string hostname and port
 * \param hostname Remote hostname to send to (as std::string pointer)
 * \param port Remote port to send to
 */
udp_client::udp_client(const std::string* hostname, int port) {
    if (hostname) {
        this->connect(hostname->c_str(), port);
    }
}

/**
 * udp_client destructor
 */
udp_client::~udp_client() {
}

/**
 * Connect with hostname and port parameters
 * \param hostname The remote host to send to
 * \param port The remote port to send to
 * \return true if setup succeeded, false otherwise
 */
bool udp_client::connect(const char* hostname, int port) {
    if (!is_valid_port(port)) {
        this->error_code = err_no_socket;
        this->error_state = state_connect;
        return false;
    }
    this->remote_host = hostname;
    this->port = port;
    return this->connect();
}

/**
 * Connect to previously set remote host and port
 * Resolves hostname and delegates to connect(const address&)
 * \return true if setup succeeded, false otherwise
 */
bool udp_client::connect() {
    struct addrinfo hints, *server_info, *connection;
    std::memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    if (!socket::is_valid_socket(this->sockfd)) {
        this->error_code = err_no_socket;
        this->error_state = state_create;
        return false;
    }

    int resolve_result = getaddrinfo(this->remote_host.c_str(), 
                                     std::to_string(this->port).c_str(),
                                     &hints, &server_info);
    if (resolve_result != 0) {
        this->error_code = err_no_host;
        this->error_state = state_connect;
        return false;
    }

    // Use the first resolved address
    connection = server_info;
    address resolved_addr(*(struct sockaddr_in*)connection->ai_addr);
    bool result = this->connect(resolved_addr);
    
    freeaddrinfo(server_info);
    return result;
}

/**
 * Connect to a specific address
 * \param addr Address object containing the target
 * \return true if setup succeeded, false otherwise
 */
bool udp_client::connect(const address& addr) {
    if (!socket::is_valid_socket(this->sockfd)) {
        this->error_code = err_no_socket;
        this->error_state = state_create;
        
        this->sockfd = ::socket(AF_INET, socket::datagram_type, 0);
        if (!socket::is_valid_socket(this->sockfd)) {
            this->error_code = err_no_socket;
            this->error_state = state_create;
            return false;
        }
    }

    this->remote_addr = addr.get_sockaddr_storage();
    this->set_socket_reuseaddr();
    this->connected = true;
    return true;
}

}  // namespace net