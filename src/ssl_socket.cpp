/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#ifdef ENABLE_SSL

#include "aeon.hpp"
#include "ssl_socket.hpp"
#include <cstring>
#include <openssl/x509v3.h>

namespace aeon {

namespace {
// Runs only when strict verification is enabled AND the caller has explicitly
// opted into trusting untrusted self-signed leaf certificates. It cannot turn a
// failed TLS handshake into success. All other chain and identity errors fail.
int verify_self_signed_leaf(int preverified, X509_STORE_CTX* ctx) {
    if (preverified == 1) return 1;
    if (X509_STORE_CTX_get_error_depth(ctx) == 0 &&
        X509_STORE_CTX_get_error(ctx) == X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT) {
        X509_STORE_CTX_set_error(ctx, X509_V_OK);
        return 1;
    }
    return 0;
}

bool numeric_ip(const std::string& name) {
    in_addr ipv4{};
    in6_addr ipv6{};
    return inet_pton(AF_INET, name.c_str(), &ipv4) == 1 ||
           inet_pton(AF_INET6, name.c_str(), &ipv6) == 1;
}
} // namespace

    ssl_socket::ssl_socket(SSL_CTX* shared_ctx)
        : socket(socket::stream_type),
          ssl_ctx(nullptr),
          ssl(nullptr),
          owns_ctx(false),
          allow_self_signed(false),
          verify_mode(ssl_verify_mode::strict) {

        if (shared_ctx != nullptr) {
            ssl_ctx = shared_ctx;
            owns_ctx = false;
        } else {
            // Create per-socket SSL context
            const SSL_METHOD* method = TLS_client_method();
            if (method == nullptr) {
                set_error(err_no_socket);
                return;
            }

            ssl_ctx = SSL_CTX_new(method);
            if (ssl_ctx == nullptr) {
                set_error(err_no_socket);
                return;
            }

            owns_ctx = true;

            // Set default verification options
            if (verify_mode == ssl_verify_mode::strict) {
                SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_PEER, nullptr);
            } else {
                SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_NONE, nullptr);
            }

            // Load system CA certificates
            if (SSL_CTX_set_default_verify_paths(ssl_ctx) != 1) {
                // Not necessarily fatal - some systems might not have a default CA bundle
                // but the connection might still work
            }
        }

        ssl = nullptr;
    }

    ssl_socket::ssl_socket(socket_t accepted_fd, SSL_CTX* server_ctx)
        : socket(accepted_fd, true),
          ssl_ctx(server_ctx), ssl(nullptr), owns_ctx(false),
          allow_self_signed(false), verify_mode(ssl_verify_mode::off) {}

    int ssl_socket::perform_server_handshake() {
        if (!ssl_ctx || !is_valid_socket()) {
            set_error(ssl_error_init);
            return ssl_error_init;
        }
#ifdef SO_NOSIGPIPE
        const int no_sigpipe = 1;
        if (::setsockopt(sockfd, SOL_SOCKET, SO_NOSIGPIPE,
                         reinterpret_cast<const char*>(&no_sigpipe),
                         sizeof(no_sigpipe)) != 0) {
            set_error(ssl_error_io);
            return ssl_error_io;
        }
#endif
        clear_ssl_errors();
        ssl = SSL_new(ssl_ctx);
        if (!ssl || SSL_set_fd(ssl, static_cast<int>(sockfd)) != 1) {
            set_error(ssl_error_init);
            return ssl_error_init;
        }
        SSL_set_accept_state(ssl);
        const int result = SSL_accept(ssl);
        if (result != 1) {
            (void)SSL_get_error(ssl, result);
            set_error(ssl_error_handshake);
            return ssl_error_handshake;
        }
        set_error(err_none);
        return err_none;
    }

    ssl_socket::~ssl_socket() {
        // Virtual dispatch in the base socket destructor no longer reaches us.
        // Release SSL while the transport is still available.
        (void)close();
        if (owns_ctx && ssl_ctx != nullptr) {
            SSL_CTX_free(ssl_ctx);
        }
        ssl_ctx = nullptr;
    }

    bool ssl_socket::close() {
        if (ssl != nullptr) {
            // Send one close_notify when possible, without waiting for the
            // peer's response. A failed/incomplete handshake is not shutdown.
            if (is_valid_socket() && SSL_is_init_finished(ssl)) {
                clear_ssl_errors();
                (void)SSL_shutdown(ssl);
            }
            SSL_free(ssl);
            ssl = nullptr;
        }
        return socket::close();
    }

    int ssl_socket::setup_ssl(const std::string& expected_identity) {
        if (ssl_ctx == nullptr) {
            set_error(ssl_error_init);
            return ssl_error_init;
        }
#ifdef SO_NOSIGPIPE
        const int no_sigpipe = 1;
        if (::setsockopt(sockfd, SOL_SOCKET, SO_NOSIGPIPE,
                         reinterpret_cast<const char*>(&no_sigpipe),
                         sizeof(no_sigpipe)) != 0) {
            set_error(ssl_error_io);
            return ssl_error_io;
        }
#endif

        // Reconnects must never retain the previous SSL object.
        if (ssl != nullptr) {
            SSL_free(ssl);
            ssl = nullptr;
        }
        clear_ssl_errors();

        // Create SSL connection object
        ssl = SSL_new(ssl_ctx);
        if (ssl == nullptr) {
            set_error(ssl_error_init);
            return ssl_error_init;
        }

        // Attach SSL to the socket file descriptor
        if (SSL_set_fd(ssl, static_cast<int>(sockfd)) != 1) {
            set_error(ssl_error_init);
            SSL_free(ssl);
            ssl = nullptr;
            return ssl_error_init;
        }

        // Load certificate if provided
        if (!cert_file.empty()) {
            if (SSL_CTX_use_certificate_file(ssl_ctx, cert_file.c_str(), SSL_FILETYPE_PEM) != 1) {
                set_error(ssl_error_cert);
                return ssl_error_cert;
            }
        }

        // Load private key if provided
        if (!key_file.empty()) {
            if (SSL_CTX_use_PrivateKey_file(ssl_ctx, key_file.c_str(), SSL_FILETYPE_PEM) != 1) {
                set_error(ssl_error_key);
                return ssl_error_key;
            }
        }

        // Per-connection settings: don't mutate a caller-supplied shared SSL_CTX.
        if (verify_mode == ssl_verify_mode::strict) {
            if (expected_identity.empty()) {
                set_error(ssl_error_cert);
                return ssl_error_cert;
            }
            SSL_set_verify(ssl, SSL_VERIFY_PEER,
                           allow_self_signed ? verify_self_signed_leaf : nullptr);
            X509_VERIFY_PARAM* params = SSL_get0_param(ssl);
            X509_VERIFY_PARAM_set_hostflags(params, X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);
            int ok = 0;
            if (numeric_ip(expected_identity)) {
                ok = X509_VERIFY_PARAM_set1_ip_asc(params, expected_identity.c_str());
            } else {
                ok = X509_VERIFY_PARAM_set1_host(params, expected_identity.c_str(), 0);
                if (ok == 1) {
                    ok = SSL_set_tlsext_host_name(ssl, expected_identity.c_str()) == 1 ? 1 : 0;
                }
            }
            if (ok != 1) {
                set_error(ssl_error_cert);
                return ssl_error_cert;
            }
        } else {
            SSL_set_verify(ssl, SSL_VERIFY_NONE, nullptr);
            // SNI is useful even with verification off, but is never proof
            // of server identity.
            if (!expected_identity.empty() && !numeric_ip(expected_identity) &&
                SSL_set_tlsext_host_name(ssl, expected_identity.c_str()) != 1) {
                set_error(ssl_error_init);
                return ssl_error_init;
            }
        }

        return err_none;
    }

    int ssl_socket::perform_handshake() {
        if (ssl == nullptr) {
            set_error(ssl_error_init);
            return ssl_error_init;
        }

        clear_ssl_errors();
        const int ret = SSL_connect(ssl);
        if (ret != 1) {
            const int ssl_err = SSL_get_error(ssl, ret);
            // Never interpret SSL_ERROR_NONE on a failed SSL_connect() as
            // success. X509 errors are verification RESULTS, not ERR reasons.
            if (verify_mode == ssl_verify_mode::strict &&
                SSL_get_verify_result(ssl) != X509_V_OK) {
                set_error(ssl_error_cert);
            } else if (ssl_err == SSL_ERROR_SYSCALL) {
                set_error(err_no_socket);
            } else {
                set_error(ssl_error_handshake);
            }
            return error_code;
        }

        if (verify_mode == ssl_verify_mode::strict &&
            SSL_get_verify_result(ssl) != X509_V_OK) {
            set_error(ssl_error_cert);
            return ssl_error_cert;
        }
        return err_none;
    }

    int ssl_socket::set_certificate_file(const char* path) {
        if (path == nullptr) {
            set_error(ssl_error);
            return ssl_error;
        }
        cert_file = path;
        return err_none;
    }

    int ssl_socket::set_private_key_file(const char* path) {
        if (path == nullptr) {
            set_error(ssl_error);
            return ssl_error;
        }
        key_file = path;
        return err_none;
    }

    int ssl_socket::set_ca_file(const char* path) {
        if (path == nullptr) {
            set_error(ssl_error);
            return ssl_error;
        }
        if (ssl_ctx == nullptr) {
            set_error(ssl_error_init);
            return ssl_error_init;
        }
        if (SSL_CTX_load_verify_locations(ssl_ctx, path, nullptr) != 1) {
            set_error(ssl_error_cert);
            return ssl_error_cert;
        }
        return err_none;
    }

    int ssl_socket::set_peer_verification(ssl_verify_mode mode) {
        // Applied to each SSL object in setup_ssl(); must not mutate a
        // shared SSL_CTX or the verification state of another connection.
        verify_mode = mode;
        return err_none;
    }

    int ssl_socket::set_allow_self_signed(bool allow) {
        allow_self_signed = allow;
        return err_none;
    }

    void ssl_socket::clear_ssl_errors() {
        while (ERR_get_error() != 0) {
            // Keep popping until queue is empty
        }
    }

    unsigned long ssl_socket::get_ssl_error() {
        return ERR_get_error();
    }

    std::string ssl_socket::get_ssl_error_string() {
        unsigned long err = ERR_get_error();
        if (err == 0) {
            return "No SSL error";
        }
        const char* err_str = ERR_reason_error_string(err);
        return err_str ? std::string(err_str) : "Unknown SSL error";
    }

    std::string ssl_socket::get_ssl_errors_all() {
        std::string errors;
        unsigned long err;
        
        while ((err = ERR_get_error()) != 0) {
            const char* err_str = ERR_reason_error_string(err);
            if (err_str) {
                errors += err_str;
                errors += "\n";
            }
        }
        
        return errors.empty() ? "No SSL errors" : errors;
    }

    int ssl_socket::write(char* data, int size) {
        if (ssl == nullptr || !connected) {
            set_error(err_no_socket);
            return err_no_socket;
        }

        clear_ssl_errors();
        
        int bytes_written = SSL_write(ssl, data, size);
        if (bytes_written <= 0) {
            int ssl_err = SSL_get_error(ssl, bytes_written);
            if (ssl_err != SSL_ERROR_NONE) {
                set_error(ssl_error_io);
            }
            return ssl_error_io;
        }

        return bytes_written;
    }

    int ssl_socket::write(const char* data, int size) {
        return write(const_cast<char*>(data), size);
    }

    int ssl_socket::write(char* data) {
        if (data == nullptr) {
            set_error(ssl_error);
            return ssl_error;
        }
        return write(data, static_cast<int>(std::strlen(data)));
    }

    int ssl_socket::write(const char* data) {
        return write(const_cast<char*>(data));
    }

    int ssl_socket::write(const std::string& data) {
        return write(const_cast<char*>(data.c_str()), static_cast<int>(data.length()));
    }

    int ssl_socket::read() {
        if (ssl == nullptr || !connected) {
            set_error(err_no_socket);
            return err_no_socket;
        }

        clear_ssl_errors();
        
        int bytes_read = SSL_read(ssl, inbuffer, max_buffer_size);
        if (bytes_read <= 0) {
            int ssl_err = SSL_get_error(ssl, bytes_read);
            if (ssl_err != SSL_ERROR_NONE) {
                set_error(ssl_error_io);
            }
            return ssl_error_io;
        }

        token_size = bytes_read;
        return bytes_read;
    }

    int ssl_socket::read(char* buffer, int size) {
        if (buffer == nullptr || size <= 0) {
            set_error(ssl_error);
            return ssl_error;
        }

        if (ssl == nullptr || !connected) {
            set_error(err_no_socket);
            return err_no_socket;
        }

        clear_ssl_errors();
        
        int bytes_read = SSL_read(ssl, buffer, size);
        if (bytes_read <= 0) {
            int ssl_err = SSL_get_error(ssl, bytes_read);
            if (ssl_err != SSL_ERROR_NONE) {
                set_error(ssl_error_io);
            }
            return ssl_error_io;
        }

        token_size = bytes_read;
        return bytes_read;
    }

    int ssl_socket::read_line(char* buffer, int size) {
        // Not optimized for SSL - reads one byte at a time until newline
        if (buffer == nullptr || size <= 0) {
            set_error(ssl_error);
            return ssl_error;
        }

        if (ssl == nullptr || !connected) {
            set_error(err_no_socket);
            return err_no_socket;
        }

        int total_read = 0;
        char byte;

        while (total_read < size - 1) {
            clear_ssl_errors();
            int bytes_read = SSL_read(ssl, &byte, 1);
            
            if (bytes_read <= 0) {
                set_error(ssl_error_io);
                return ssl_error_io;
            }

            buffer[total_read++] = byte;
            
            if (byte == '\n') {
                break;
            }
        }

        buffer[total_read] = '\0';
        return total_read;
    }

    int ssl_socket::read_until(char* buffer, int size) {
        // For SSL, same as read_line for now (reads until newline)
        return read_line(buffer, size);
    }

    std::string ssl_socket::read(int size) {
        if (size <= 0) {
            return "";
        }

        char* temp_buffer = new char[size + 1];
        int bytes_read = read(temp_buffer, size);
        
        std::string result;
        if (bytes_read > 0) {
            result = std::string(temp_buffer, bytes_read);
        }
        
        delete[] temp_buffer;
        return result;
    }

} // aeon

#endif // ENABLE_SSL