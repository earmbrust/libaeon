/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2026 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/
#pragma once
#include "aeon_features.hpp"
#if defined(ENABLE_SSL) && LIBAEON_HAS_EVENTS
#include "event_socket.hpp"
#include "ssl_socket.hpp" // Public TLS error codes and verification modes.
#include <openssl/ssl.h>
#include <chrono>
#include <cstdint>

namespace aeon {
class ssl_server_event_socket;

/** Nonblocking TLS transport using the ordinary event_socket callbacks,
 *  queues, and event_socket_set polling. The TCP connect phase may block,
 *  like client_event_socket::connect; the TLS handshake never blocks.
 *  on_connect fires after the TLS handshake, not after bare TCP connect.
 */
class ssl_event_socket : public event_socket {
    friend class ssl_server_event_socket;
public:
    LIBAEON_API ssl_event_socket();
    LIBAEON_API ~ssl_event_socket() override;

    LIBAEON_API int set_ca_file(const char* path);
    LIBAEON_API int set_peer_verification(ssl_verify_mode mode);
    LIBAEON_API int set_allow_self_signed(bool allow);
    LIBAEON_API bool connect(const char* hostname, int port);
    LIBAEON_API bool connect(const address& addr);
    LIBAEON_API bool handshake_complete() const noexcept;
    // The deadline starts with each handshake. Valid range: 100-60000 ms.
    // Changing the limit during negotiation restarts the deadline from now.
    LIBAEON_API int set_handshake_timeout(int milliseconds);
    // Opt-in, poll-driven shutdown: stop new writes, drain queued application
    // data, send close_notify, and wait for peer close_notify until the deadline.
    // Does not block; registered event_socket_set::poll() drives completion.
    // Returns false if TLS is not ready or a direct SSL_write retry is pending.
    // Explicit close() still immediately discards queued output.
    LIBAEON_API bool begin_graceful_shutdown(int timeout_ms = 5000);
    LIBAEON_API bool graceful_shutdown_pending() const noexcept;
    LIBAEON_API bool close() override;
    // Every inherited socket read must decrypt; raw socket::read is forbidden
    // for TLS even when the caller does not use event_socket_set.
    LIBAEON_API int read() override;
    LIBAEON_API int read(char* buffer, int size) override;
    LIBAEON_API int read_line(char* buffer, int size) override;
    LIBAEON_API int read_until(char* buffer, int size) override;
    LIBAEON_API std::string read(int size) override;

protected:
    LIBAEON_API int poll_receive(char* buffer, int capacity) override;
    LIBAEON_API int poll_send(const char* data, int size, int flags) override;
    LIBAEON_API int poll_send_queued(const char* data, int size, int flags) override;
    LIBAEON_API bool poll_wants_write() const override;
    LIBAEON_API bool poll_accepts_queued_write() const override;
    LIBAEON_API int poll_step(short readiness) override;
    LIBAEON_API bool poll_read_ready(short readiness) const override;
    LIBAEON_API bool poll_queue_ready(short readiness) const override;
    LIBAEON_API bool poll_has_buffered_input() const override;
    LIBAEON_API int poll_timeout_ms() const override;
    LIBAEON_API bool poll_error_terminal() const override;

private:
    // Accept an already-owned descriptor with a retained shared server context.
    ssl_event_socket(socket_t accepted_fd, SSL_CTX* server_ctx);
    bool init_session(bool server);
    bool start_client(const std::string& expected_identity);
    int send_tls(const char* data, int size, bool queued);
    static void set_would_block();
    static void set_transport_error();

    SSL_CTX* ctx_ = nullptr;
    SSL* session_ = nullptr;
    bool server_side_ = false;
    bool tls_ready_ = false;
    bool need_write_ = false;   // handshake or read needs writable descriptor
    bool send_need_read_ = false;
    bool recv_need_write_ = false;
    int pending_write_len_ = 0;
    bool ssl_failed_ = false;
    bool peer_notified_close_ = false;
    bool shutdown_requested_ = false;
    bool shutdown_notify_sent_ = false;
    bool shutdown_need_write_ = false;
    bool shutdown_need_read_ = false;
    std::chrono::steady_clock::time_point shutdown_deadline_{};
    bool pending_write_queued_ = false;
    std::uint64_t pending_write_hash_ = 0;
    int handshake_timeout_ms_ = 5000;
    std::chrono::steady_clock::time_point handshake_deadline_{}; // retry SSL_write with the original byte count
    ssl_verify_mode verify_mode_ = ssl_verify_mode::strict;
    bool allow_self_signed_ = false;
    std::string expected_identity_;
};
} // namespace aeon
#endif
