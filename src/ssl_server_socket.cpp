/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2026 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/
#ifdef ENABLE_SSL
#include "aeon.hpp"
#include "ssl_server_socket.hpp"
#include <memory>

namespace aeon {
namespace {
// No extra public peer type: the server-owned peer is an ssl_socket with
// access to the base class's protected adopted-descriptor constructor.
class ssl_server_peer final : public ssl_socket {
public:
    ssl_server_peer(socket_t fd, SSL_CTX* ctx) : ssl_socket(fd, ctx) {}
    int handshake() { return perform_server_handshake(); }
};
} // namespace

ssl_server_socket::ssl_server_socket() : server_ctx_(SSL_CTX_new(TLS_server_method())) {
    if (!server_ctx_) set_error(ssl_error_init);
}

ssl_server_socket::~ssl_server_socket() {
    // Close all TLS sessions while the shared SSL_CTX is still alive.
    (void)server_socket::close();
    if (server_ctx_) SSL_CTX_free(server_ctx_);
    server_ctx_ = nullptr;
}

int ssl_server_socket::set_certificate_file(const char* pem_path) {
    if (!server_ctx_ || !pem_path || !*pem_path ||
        SSL_CTX_use_certificate_chain_file(server_ctx_, pem_path) != 1) {
        set_error(ssl_error_cert);
        return ssl_error_cert;
    }
    set_error(err_none);
    return err_none;
}

int ssl_server_socket::set_private_key_file(const char* pem_path) {
    if (!server_ctx_ || !pem_path || !*pem_path ||
        SSL_CTX_use_PrivateKey_file(server_ctx_, pem_path, SSL_FILETYPE_PEM) != 1) {
        set_error(ssl_error_key);
        return ssl_error_key;
    }
    set_error(err_none);
    return err_none;
}

int ssl_server_socket::set_handshake_timeout(int milliseconds) {
    if (milliseconds < 100 || milliseconds > 60000) {
        set_error(ssl_error);
        return ssl_error;
    }
    handshake_timeout_ms_ = milliseconds;
    set_error(err_none);
    return err_none;
}

bool ssl_server_socket::listen(const char* address, int port) {
    if (!server_ctx_ || SSL_CTX_check_private_key(server_ctx_) != 1) {
        set_error(ssl_error_cert);
        return false;
    }
    return server_socket::listen(address, port);
}

ssl_socket* ssl_server_socket::accept() { return accept(true); }

ssl_socket* ssl_server_socket::accept(bool blocking) {
    // The inherited accept path handles address metadata, parent association,
    // socket_set membership, and ownership for both encrypted and plain peers.
    return static_cast<ssl_socket*>(server_socket::accept(blocking));
}

std::unique_ptr<socket> ssl_server_socket::create_accepted_socket(socket_t fd) {
    auto peer = std::make_unique<ssl_server_peer>(fd, server_ctx_);
    // The accepted transport must be blocking for the synchronous TLS handshake,
    // even when the caller requests a nonblocking peer after the handshake.
    if (peer->set_blocking(true) != err_none ||
        peer->set_read_timeout(handshake_timeout_ms_) != err_none ||
        peer->set_write_timeout(handshake_timeout_ms_) != err_none) {
        set_error(ssl_error_io);
        return nullptr;
    }
    if (peer->handshake() != err_none) {
        set_error(peer->get_error());
        return nullptr;
    }
    // Restore the ordinary peer timeout defaults; the inherited accept path
    // then applies the caller-requested blocking mode.
    if (peer->set_read_timeout(0) != err_none ||
        peer->set_write_timeout(0) != err_none) {
        set_error(ssl_error_io);
        return nullptr;
    }
    set_error(err_none);
    return peer;
}

} // namespace aeon
#endif // ENABLE_SSL
