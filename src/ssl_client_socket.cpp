/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#ifdef ENABLE_SSL

#include "aeon.hpp"
#include "native_error.hpp"
#include "ssl_client_socket.hpp"
#include "resolver.hpp"
#include <stdexcept>

namespace aeon {

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
        // connect(hostname.c_str(), ...) would alias the same string that
        // connect(const char*, ...) updates. Take an independent copy.
        const std::string requested = hostname;
        return connect(requested.c_str(), port_num);
    }

    bool ssl_client_socket::connect(const char* hostname_arg, int remote_port) {
        if (hostname_arg == nullptr || hostname_arg[0] == '\0' ||
            remote_port <= 0 || remote_port > 65535) {
            set_error(ssl_error);
            return false;
        }

        const std::string requested = hostname_arg;
        hostname = requested;
        port_num = remote_port;

        try {
            resolver res;
            address addr = res.resolve_to_address(requested.c_str());
            addr.set_port(static_cast<std::uint16_t>(remote_port));
            return connect_to(addr, requested);
        } catch (const std::exception&) {
            set_error(err_no_host);
            return false;
        }
    }

    bool ssl_client_socket::connect(const address& addr) {
        // Direct-address connections verify the numeric address, never a
        // hostname retained from a previous connection attempt.
        if ((!addr.is_ipv4() && !addr.is_ipv6()) || addr.get_port() == 0) {
            set_error(ssl_error);
            return false;
        }
        const std::string requested = addr.get_string();
        hostname = requested;
        port_num = addr.get_port();
        return connect_to(addr, requested);
    }

    bool ssl_client_socket::connect_to(const address& addr,
                                       const std::string& expected_identity) {
        const sockaddr_storage target_addr = addr.get_sockaddr_storage();
        const int target_family = addr.is_ipv6() ? AF_INET6 : AF_INET;
        const socklen_t addr_len = addr.is_ipv6() ? sizeof(sockaddr_in6) : sizeof(sockaddr_in);

        // A previous connection or handshake must not leave a live SSL object
        // or connected descriptor attached to this instance.
        if (ssl != nullptr || connected) {
            (void)close();
        }
        if (is_valid_socket() && target_family != net_family) {
            (void)socket::close();
        }
        if (!is_valid_socket()) {
            sockfd = ::socket(target_family, socket::stream_type, 0);
            if (!is_valid_socket()) {
                set_error(err_no_socket);
                return false;
            }
            net_family = target_family;
            configure_socket_for_connect();
        }

        if (::connect(sockfd, reinterpret_cast<const sockaddr*>(&target_addr), addr_len) != 0) {
            set_error(translate_native_error(GET_NET_SOCKET_ERROR()));
            (void)close();
            return false;
        }

        connected = true;
        clear_buffers();
        port = addr.get_port();
        set_remote_hostname(expected_identity);

        if (setup_ssl(expected_identity) != err_none || perform_handshake() != err_none) {
            const int failure = error_code;
            (void)close();
            set_error(failure);
            return false;
        }

        set_error(err_none);
        return true;
    }

} // aeon

#endif // ENABLE_SSL
