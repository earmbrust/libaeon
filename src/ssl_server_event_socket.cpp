/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2026 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/
#include "aeon.hpp"
#if defined(ENABLE_SSL) && LIBAEON_HAS_EVENTS
#include "ssl_server_event_socket.hpp"
#include <memory>

namespace aeon {
ssl_server_event_socket::ssl_server_event_socket()
    : ctx_(SSL_CTX_new(TLS_server_method())) {
    if (!ctx_) set_error(ssl_error_init);
}
ssl_server_event_socket::~ssl_server_event_socket() {
    (void)server_event_socket::close();
    if (ctx_) SSL_CTX_free(ctx_);
}
int ssl_server_event_socket::set_certificate_file(const char* pem) {
    if (!ctx_ || !pem || !*pem || SSL_CTX_use_certificate_chain_file(ctx_, pem) != 1) {
        set_error(ssl_error_cert);
        return ssl_error_cert;
    }
    set_error(err_none);
    return err_none;
}
int ssl_server_event_socket::set_private_key_file(const char* pem) {
    if (!ctx_ || !pem || !*pem ||
        SSL_CTX_use_PrivateKey_file(ctx_, pem, SSL_FILETYPE_PEM) != 1) {
        set_error(ssl_error_key);
        return ssl_error_key;
    }
    set_error(err_none);
    return err_none;
}
bool ssl_server_event_socket::listen(const char* ip, int port) {
    if (!ctx_ || SSL_CTX_check_private_key(ctx_) != 1) {
        set_error(ssl_error_cert);
        return false;
    }
    return server_event_socket::listen(ip, port);
}
ssl_event_socket* ssl_server_event_socket::accept() { return accept(false); }
ssl_event_socket* ssl_server_event_socket::accept(bool blocking) {
    if (blocking) { // Handshakes always run asynchronously through poll().
        set_error(ssl_error);
        return nullptr;
    }
    return static_cast<ssl_event_socket*>(server_event_socket::accept(false));
}
std::unique_ptr<event_socket> ssl_server_event_socket::create_accepted_socket(socket_t fd) {
    auto peer = std::unique_ptr<ssl_event_socket>(new ssl_event_socket(fd, ctx_));
    if (!peer->is_valid_socket() || !peer->session_) {
        set_error(ssl_error_init);
        return {};
    }
    return peer;
}
} // namespace aeon
#endif
