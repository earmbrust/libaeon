/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#ifdef ENABLE_SSL

#include "ssl_client_socket.h"
#include "resolver.h"
#include <stdexcept>

namespace net {

    ssl_client_socket::ssl_client_socket()
        : ssl_socket(nullptr),
          hostname(""),
          port_num(0) {
    }

    ssl_client_socket::ssl_client_socket(const char* hostname_arg, int port)
        : ssl_socket(nullptr),
          hostname(hostname_arg ? hostname_arg : ""),
          port_num(port) {
        this->connect(hostname_arg, port);
    }

    ssl_client_socket::ssl_client_socket(SSL_CTX* shared_ctx)
        : ssl_socket(shared_ctx),
          hostname(""),
          port_num(0) {
    }

    ssl_client_socket::ssl_client_socket(const char* hostname_arg, int port, SSL_CTX* shared_ctx)
        : ssl_socket(shared_ctx),
          hostname(hostname_arg ? hostname_arg : ""),
          port_num(port) {
        this->connect(hostname_arg, port);
    }

    ssl_client_socket::~ssl_client_socket() {
    }

    bool ssl_client_socket::connect() {
        if (hostname.empty() || port_num <= 0) {
            set_error(ssl_error);
            return false;
        }

        return connect(hostname.c_str(), port_num);
    }

    bool ssl_client_socket::connect(const char* hostname_arg, int remote_port) {
        if (hostname_arg == nullptr || remote_port <= 0) {
            set_error(ssl_error);
            return false;
        }

        hostname = hostname_arg;
        port_num = remote_port;

        // Use resolver to get address
        try {
            resolver res;
            address addr = res.resolve_to_address(hostname_arg);
            addr.set_port(static_cast<std::uint16_t>(remote_port));
            return connect(addr);
        } catch (const std::exception&) {
            set_error(err_no_host);
            return false;
        }
    }

    bool ssl_client_socket::connect(const address& addr) {
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
                set_error(err_no_socket);
                return false;
            }

            this->configure_socket_for_connect();
            this->net_family = target_family;
        }

        // Standard blocking connect
        if (::connect(this->sockfd, (struct sockaddr*)&target_addr, addr_len) == 0) {
            this->connected = true;
            this->clear_buffers();
            this->port = addr.get_port();
            this->remote_host = hostname;

            // Now set up SSL/TLS
            if (setup_ssl() != err_none) {
                close();
                return false;
            }

            // Perform SSL handshake
            if (perform_handshake() != err_none) {
                close();
                return false;
            }

            return true;
        }

        set_error(GET_NET_SOCKET_ERROR());
        this->connected = false;
        NET_CLOSE_SOCKET(this->sockfd);
        this->sockfd = invalid_socket;
        return false;
    }

} // namespace net

#endif // ENABLE_SSL