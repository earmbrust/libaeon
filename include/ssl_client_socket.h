/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#pragma once

#ifdef _MSC_VER
#pragma warning(disable:4251)
#endif

#ifdef ENABLE_SSL

#include "ssl_socket.h"

namespace net {

    /**
     * \class ssl_client_socket
     * \brief SSL/TLS enabled client socket for HTTPS and other secure connections
     * 
     * Inherits from ssl_socket and extends client_socket functionality with SSL/TLS.
     * Automatically handles SSL handshake on connect.
     */
    class LIBAEON_API ssl_client_socket : public ssl_socket {
    public:
        /**
         * \brief Default constructor
         */
        ssl_client_socket();

        /**
         * \brief Constructor with hostname and port
         * \param hostname Remote hostname to connect to
         * \param port Remote port to connect to
         */
        explicit ssl_client_socket(const char* hostname, int port);

        /**
         * \brief Constructor with shared SSL context
         * \param shared_ctx Existing SSL_CTX to use (not freed by this socket)
         */
        explicit ssl_client_socket(SSL_CTX* shared_ctx);

        /**
         * \brief Constructor with hostname, port, and shared context
         * \param hostname Remote hostname to connect to
         * \param port Remote port to connect to
         * \param shared_ctx Existing SSL_CTX to use (not freed by this socket)
         */
        explicit ssl_client_socket(const char* hostname, int port, SSL_CTX* shared_ctx);

        /**
         * \brief Connect to remote host without hostname/port (use previously set values)
         * \return true if connection and SSL handshake successful, false otherwise
         */
        bool connect();

        /**
         * \brief Connect to remote host
         * \param hostname Remote hostname to connect to
         * \param port Remote port to connect to
         * \return true if connection and SSL handshake successful, false otherwise
         */
        bool connect(const char* hostname, int port);

        /**
         * \brief Connect using address structure
         * \param addr Address object containing connection details
         * \return true if connection and SSL handshake successful, false otherwise
         */
        bool connect(const address& addr);

        virtual ~ssl_client_socket();

    private:
        std::string hostname;
        int port_num;
    };

} // namespace net

#endif // ENABLE_SSL