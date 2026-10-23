/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#pragma once

#ifdef ENABLE_SSL

#include "socket.hpp"
#include <openssl/ssl.h>
#include <openssl/err.h>

namespace aeon {

    // SSL/TLS error codes (negative to distinguish from socket errors)
    constexpr int ssl_error = -1;          // Generic SSL error
    constexpr int ssl_error_init = -2;     // SSL context/object creation failed
    constexpr int ssl_error_cert = -3;     // Certificate verification/loading failed
    constexpr int ssl_error_key = -4;      // Private key loading failed
    constexpr int ssl_error_handshake = -5; // SSL handshake failed
    constexpr int ssl_error_io = -6;       // SSL I/O error

    /**
     * \brief SSL/TLS peer verification mode
     */
    enum class ssl_verify_mode {
        strict,  // Require a trusted certificate matching the requested DNS name or IP
        off      // Don't verify peer certificate
    };

    /**
     * \class ssl_socket
     * \brief Base class for SSL/TLS enabled sockets
     * 
     * Inherits from socket and adds OpenSSL/TLS encryption.
     * Should not be instantiated directly; use ssl_client_socket or ssl_server_socket.
     */
    class LIBAEON_API ssl_socket : public socket {
    protected:
        SSL_CTX* ssl_ctx;
        SSL* ssl;
        bool owns_ctx;
        bool allow_self_signed;
        ssl_verify_mode verify_mode;
        std::string cert_file;
        std::string key_file;

        // Protected constructor for derived classes
        explicit ssl_socket(SSL_CTX* shared_ctx = nullptr);

        // Initialize SSL, attach its descriptor and verify the expected peer identity.
        int setup_ssl(const std::string& expected_identity);

        // Perform SSL/TLS handshake
        int perform_handshake();

        // Clear OpenSSL error queue before operations
        void clear_ssl_errors();

    public:
        virtual ~ssl_socket();

        // Error handling - analogs to OpenSSL's ERR_* functions
        unsigned long get_ssl_error();
        std::string get_ssl_error_string();
        std::string get_ssl_errors_all();

        // Configuration methods (call before connect)
        int set_certificate_file(const char* path);
        int set_private_key_file(const char* path);
        int set_ca_file(const char* path);
        int set_peer_verification(ssl_verify_mode mode);
        // Explicitly allow an untrusted self-signed LEAF certificate in strict mode.
        // Peer identity (DNS/IP), certificate validity and handshake are still checked.
        int set_allow_self_signed(bool allow);

        // Close TLS before releasing the owned native socket; safe on failure/retry.
        bool close() override;

        // Override read/write to use SSL
        int write(char* data, int size) override;
        int write(const char* data, int size) override;
        int write(char* data) override;
        int write(const char* data) override;
        int write(const std::string& data) override;

        int read() override;
        int read(char* buffer, int size) override;
        int read_line(char* buffer, int size) override;
        int read_until(char* buffer, int size) override;
        std::string read(int size) override;
    };

} // aeon

#endif // ENABLE_SSL