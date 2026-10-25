/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2026 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/
#pragma once
#ifdef ENABLE_SSL
#include "server_socket.hpp"
#include "ssl_socket.hpp"

namespace aeon {

/** A TCP listener whose accepted peers use TLS. Plain socket classes remain
 *  fully available when SSL support is compiled in. */
class ssl_server_socket : public server_socket {
public:
    LIBAEON_API ssl_server_socket();
    LIBAEON_API ~ssl_server_socket() override;

    // Install server credentials before listen(). Both files must be present
    // and contain a matching certificate/private-key pair.
    LIBAEON_API int set_certificate_file(const char* pem_path);
    LIBAEON_API int set_private_key_file(const char* pem_path);

    // Each handshake's blocking socket reads/writes time out after this many
    // milliseconds. Requiring a positive value prevents unbounded idle peers.
    LIBAEON_API int set_handshake_timeout(int milliseconds);

    using server_socket::listen;
    LIBAEON_API bool listen(const char* address, int port) override;
    LIBAEON_API ssl_socket* accept() override;
    LIBAEON_API ssl_socket* accept(bool blocking) override;

protected:
    std::unique_ptr<socket> create_accepted_socket(socket_t fd) override;

private:
    SSL_CTX* server_ctx_ = nullptr;
    int handshake_timeout_ms_ = 5000;
};

} // namespace aeon
#endif // ENABLE_SSL
