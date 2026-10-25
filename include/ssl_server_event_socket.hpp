/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2026 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/
#pragma once
#if defined(ENABLE_SSL) && LIBAEON_HAS_EVENTS
#include "server_event_socket.hpp"
#include "ssl_event_socket.hpp"

namespace aeon {
/** Nonblocking TLS listener. Accepted ssl_event_socket peers are owned by
 *  this listener and must be removed from external event_socket_sets before
 *  their destruction. Handshakes progress when the application calls poll(). */
class ssl_server_event_socket : public server_event_socket {
public:
    LIBAEON_API ssl_server_event_socket();
    LIBAEON_API ~ssl_server_event_socket() override;
    LIBAEON_API int set_certificate_file(const char* pem);
    LIBAEON_API int set_private_key_file(const char* pem);
    using server_event_socket::listen;
    LIBAEON_API bool listen(const char* ip, int port) override;
    LIBAEON_API ssl_event_socket* accept() override;
    LIBAEON_API ssl_event_socket* accept(bool blocking) override;

protected:
    std::unique_ptr<event_socket> create_accepted_socket(socket_t fd) override;

private:
    SSL_CTX* ctx_ = nullptr;
};
} // namespace aeon
#endif
