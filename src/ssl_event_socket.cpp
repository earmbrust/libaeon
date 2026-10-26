/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2026 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/
#include "aeon.hpp"
#ifndef NET_PLATFORM_WINDOWS
#include <poll.h>
#endif
#if defined(ENABLE_SSL) && LIBAEON_HAS_EVENTS
#include "ssl_event_socket.hpp"
#include <openssl/x509v3.h>
#include <algorithm>
#include <chrono>

namespace aeon {
namespace {
int allow_self_signed_leaf(int ok, X509_STORE_CTX* ctx) {
    if (ok) return 1;
    if (X509_STORE_CTX_get_error_depth(ctx) == 0 &&
        X509_STORE_CTX_get_error(ctx) == X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT) {
        X509_STORE_CTX_set_error(ctx, X509_V_OK);
        return 1;
    }
    return 0;
}
bool is_numeric_ip(const std::string& text) {
    in_addr v4{};
    in6_addr v6{};
    return inet_pton(AF_INET, text.c_str(), &v4) == 1 ||
           inet_pton(AF_INET6, text.c_str(), &v6) == 1;
}
// OpenSSL requires the same plaintext on every retry after WANT_READ/WRITE,
// even when SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER permits a new address.
std::uint64_t bytes_hash(const char* data, int size) {
    std::uint64_t hash = UINT64_C(14695981039346656037);
    for (int i = 0; i < size; ++i) {
        hash ^= static_cast<unsigned char>(data[i]);
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}
}

ssl_event_socket::ssl_event_socket() : ctx_(SSL_CTX_new(TLS_client_method())) {
    if (!ctx_) set_error(ssl_error_init);
    else (void)SSL_CTX_set_default_verify_paths(ctx_);
}

ssl_event_socket::ssl_event_socket(socket_t fd, SSL_CTX* server_ctx)
    : event_socket(fd, true), ctx_(server_ctx), server_side_(true),
      verify_mode_(ssl_verify_mode::off) {
    if (!ctx_ || SSL_CTX_up_ref(ctx_) != 1) {
        ctx_ = nullptr;
        set_error(ssl_error_init);
        return;
    }
    if (!init_session(true)) set_error(ssl_error_init);
}

ssl_event_socket::~ssl_event_socket() {
    (void)close();
    if (ctx_) SSL_CTX_free(ctx_);
}

void ssl_event_socket::set_would_block() {
#ifdef NET_PLATFORM_WINDOWS
    WSASetLastError(WSAEWOULDBLOCK);
#else
    errno = EWOULDBLOCK;
#endif
}
void ssl_event_socket::set_transport_error() {
#ifdef NET_PLATFORM_WINDOWS
    WSASetLastError(WSAECONNABORTED);
#else
    errno = EPROTO;
#endif
}

bool ssl_event_socket::init_session(bool server) {
    if (!ctx_ || !is_valid_socket()) return false;
    if (session_) { SSL_free(session_); session_ = nullptr; }
    ERR_clear_error();
    session_ = SSL_new(ctx_);
    if (!session_ || SSL_set_fd(session_, static_cast<int>(sockfd)) != 1) return false;
    SSL_set_mode(session_, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER |
                           SSL_MODE_ENABLE_PARTIAL_WRITE);
    if (server) SSL_set_accept_state(session_);
    else SSL_set_connect_state(session_);
    tls_ready_ = false;
    need_write_ = !server;
    send_need_read_ = false;
    recv_need_write_ = false;
    pending_write_len_ = 0;
    pending_write_queued_ = false;
    pending_write_hash_ = 0;
    ssl_failed_ = false;
    peer_notified_close_ = false;
    handshake_deadline_ = std::chrono::steady_clock::now() +
        std::chrono::milliseconds(handshake_timeout_ms_);
    return true;
}

int ssl_event_socket::set_ca_file(const char* path) {
    if (!ctx_ || !path || !*path ||
        SSL_CTX_load_verify_locations(ctx_, path, nullptr) != 1) {
        set_error(ssl_error_cert);
        return ssl_error_cert;
    }
    set_error(err_none);
    return err_none;
}
int ssl_event_socket::set_peer_verification(ssl_verify_mode mode) {
    verify_mode_ = mode;
    return err_none;
}
int ssl_event_socket::set_allow_self_signed(bool enabled) {
    allow_self_signed_ = enabled;
    return err_none;
}

bool ssl_event_socket::start_client(const std::string& identity) {
    expected_identity_ = identity;
    if (!is_valid_socket() || set_blocking(false) != err_none || !init_session(false)) {
        set_error(ssl_error_init);
        (void)close();
        return false;
    }
    if (verify_mode_ == ssl_verify_mode::strict) {
        SSL_set_verify(session_, SSL_VERIFY_PEER,
                       allow_self_signed_ ? allow_self_signed_leaf : nullptr);
        auto* params = SSL_get0_param(session_);
        X509_VERIFY_PARAM_set_hostflags(params, X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);
        const int result = is_numeric_ip(identity)
            ? X509_VERIFY_PARAM_set1_ip_asc(params, identity.c_str())
            : X509_VERIFY_PARAM_set1_host(params, identity.c_str(), 0);
        if (result != 1) {
            set_error(ssl_error_cert);
            (void)close();
            return false;
        }
    } else SSL_set_verify(session_, SSL_VERIFY_NONE, nullptr);
    if (!is_numeric_ip(identity) && SSL_set_tlsext_host_name(session_, identity.c_str()) != 1) {
        set_error(ssl_error_init);
        (void)close();
        return false;
    }
    set_error(err_none);
    return true;
}

bool ssl_event_socket::connect(const char* hostname, int port) {
    if (server_side_ || !hostname || !*hostname || port < 1 || port > 65535) {
        set_error(ssl_error);
        return false;
    }
    const std::string requested(hostname);
    // Explicitly tear down an earlier TLS session before replacing its TCP
    // descriptor. A failed reconnect must not leave a ready stale session.
    if (session_ || connected) (void)close();
    // Match existing client_event_socket: TCP connection is synchronous,
    // but TLS handshake is always progressed by event_socket_set::poll().
    (void)set_blocking(true);
    if (!tcp_connect(requested.c_str(), port)) return false;
    return start_client(requested);
}
bool ssl_event_socket::connect(const address& addr) {
    if (server_side_ || (!addr.is_ipv4() && !addr.is_ipv6()) || addr.get_port() == 0) {
        set_error(ssl_error);
        return false;
    }
    const std::string identity = addr.get_string();
    if (session_ || connected) (void)close();
    (void)set_blocking(true);
    if (!tcp_connect(addr)) return false;
    return start_client(identity);
}

bool ssl_event_socket::handshake_complete() const noexcept { return tls_ready_; }

int ssl_event_socket::set_handshake_timeout(int milliseconds) {
    if (milliseconds < 100 || milliseconds > 60000) {
        set_error(ssl_error);
        return ssl_error;
    }
    handshake_timeout_ms_ = milliseconds;
    if (session_ && !tls_ready_)
        handshake_deadline_ = std::chrono::steady_clock::now() +
            std::chrono::milliseconds(milliseconds);
    set_error(err_none);
    return err_none;
}

int ssl_event_socket::poll_timeout_ms() const {
    if (!session_ || tls_ready_ || !is_valid_socket()) return -1;
    const auto now = std::chrono::steady_clock::now();
    if (now >= handshake_deadline_) return 0;
    // Round up, not down: a deadline less than one millisecond away must
    // not generate a busy 0ms poll loop.
    const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
        handshake_deadline_ - now).count();
    return static_cast<int>(remaining + 1);
}

bool ssl_event_socket::close() {
    if (session_) {
        // Nonblocking close: do not wait for a peer's close_notify.
        // No application data may be sent after transport teardown begins.
        if (tls_ready_ && !ssl_failed_ && !peer_notified_close_ && is_valid_socket()) {
            ERR_clear_error();
            (void)SSL_shutdown(session_);
        }
        SSL_free(session_);
        session_ = nullptr;
    }
    tls_ready_ = false;
    pending_write_len_ = 0;
    pending_write_queued_ = false;
    pending_write_hash_ = 0;
    ssl_failed_ = false;
    peer_notified_close_ = false;
    need_write_ = false;
    send_need_read_ = false;
    recv_need_write_ = false;
    return event_socket::close();
}

int ssl_event_socket::poll_step(short flags) {
    // Check deadlines even when WSAPoll/poll returned no socket events.
    if (session_ && !tls_ready_ &&
        std::chrono::steady_clock::now() >= handshake_deadline_) {
        set_error(ssl_error_handshake);
        return -1;
    }
    if (!session_) { set_error(ssl_error_init); return -1; }
    if (tls_ready_) {
        // POLLHUP can arrive alongside a TLS close_notify. Allow SSL_read to
        // distinguish an orderly TLS close from truncated ciphertext.
        if (flags & (POLLERR | POLLNVAL)) {
            ssl_failed_ = true;
            set_error(ssl_error_io);
            return -1;
        }
        return 1;
    }
    if (flags & (POLLERR | POLLNVAL | POLLHUP)) {
        set_error(ssl_error_handshake);
        return -1;
    }
    if ((need_write_ && !(flags & POLLOUT)) ||
        (!need_write_ && !(flags & (POLLIN | POLLHUP)))) return 0;
    ERR_clear_error();
    const int result = SSL_do_handshake(session_);
    if (result == 1) {
        if (!server_side_ && verify_mode_ == ssl_verify_mode::strict &&
            SSL_get_verify_result(session_) != X509_V_OK) {
            set_error(ssl_error_cert);
            return -1;
        }
        tls_ready_ = true;
        need_write_ = false;
        set_error(err_none);
        fire_on_connect();
        // The socket-set-level connect callback is fired by poll().
        return 2;
    }
    const int err = SSL_get_error(session_, result);
    if (err == SSL_ERROR_WANT_READ) { need_write_ = false; return 0; }
    if (err == SSL_ERROR_WANT_WRITE) { need_write_ = true; return 0; }
    if (!server_side_ && verify_mode_ == ssl_verify_mode::strict &&
        SSL_get_verify_result(session_) != X509_V_OK) set_error(ssl_error_cert);
    else set_error(ssl_error_handshake);
    return -1;
}

int ssl_event_socket::poll_receive(char* data, int size) {
    if (!tls_ready_ || !session_) { set_transport_error(); return -1; }
    ERR_clear_error();
    const int result = SSL_read(session_, data, size);
    if (result > 0) { recv_need_write_ = false; return result; }
    const int code = SSL_get_error(session_, result);
    if (code == SSL_ERROR_ZERO_RETURN) {
        // The peer sent a TLS close_notify; don't try to write a second alert
        // after its transport may already have closed.
        peer_notified_close_ = true;
        return 0;
    }
    if (code == SSL_ERROR_WANT_READ || code == SSL_ERROR_WANT_WRITE) {
        recv_need_write_ = code == SSL_ERROR_WANT_WRITE;
        set_would_block();
        return -1;
    }
    ssl_failed_ = true;
    set_error(ssl_error_io);
    set_transport_error();
    return -1;
}
int ssl_event_socket::poll_send(const char* data, int size, int) {
    return send_tls(data, size, false);
}
int ssl_event_socket::poll_send_queued(const char* data, int size, int) {
    return send_tls(data, size, true);
}
int ssl_event_socket::send_tls(const char* data, int size, bool queued) {
    if (!tls_ready_ || !session_ || !data || size <= 0) {
        set_transport_error();
        return -1;
    }
    // Direct writes must not jump ahead of already accepted queued bytes.
    // In particular, OpenSSL must never retry a queued write using a direct
    // write's buffer (or the other way around) after WANT_READ/WANT_WRITE.
    if (!queued && !pending_write_len_ && queued_bytes() != 0) {
        set_would_block();
        return -1;
    }
    if (pending_write_len_) {
        if (queued != pending_write_queued_) {
            set_would_block();
            return -1;
        }
        if (size < pending_write_len_ ||
            (!queued && size != pending_write_len_) ||
            bytes_hash(data, pending_write_len_) != pending_write_hash_) {
            set_error(ssl_error_io);
            set_transport_error();
            return -1;
        }
    }
    const int requested = pending_write_len_ ? pending_write_len_ : size;
    ERR_clear_error();
    const int result = SSL_write(session_, data, requested);
    if (result > 0) {
        pending_write_len_ = 0;
        pending_write_queued_ = false;
        pending_write_hash_ = 0;
        send_need_read_ = false;
        return result;
    }
    const int code = SSL_get_error(session_, result);
    if (code == SSL_ERROR_WANT_READ || code == SSL_ERROR_WANT_WRITE) {
        if (!pending_write_len_) {
            pending_write_len_ = requested;
            pending_write_queued_ = queued;
            pending_write_hash_ = bytes_hash(data, requested);
        }
        send_need_read_ = code == SSL_ERROR_WANT_READ;
        set_would_block();
        return -1;
    }
    pending_write_len_ = 0;
    pending_write_queued_ = false;
    pending_write_hash_ = 0;
    send_need_read_ = false;
    ssl_failed_ = true;
    set_error(ssl_error_io);
    set_transport_error();
    return -1;
}
bool ssl_event_socket::poll_wants_write() const {
    if (!tls_ready_) return need_write_;
    // A pending direct SSL_write belongs to the caller. Never wake the poller
    // just to consume its incomplete record with buffered output.
    return recv_need_write_ || (!send_need_read_ &&
        !(pending_write_len_ && !pending_write_queued_) && queued_bytes() != 0);
}
bool ssl_event_socket::poll_read_ready(short readiness) const {
    return (!send_need_read_ && (readiness & (POLLIN | POLLHUP))) ||
           (recv_need_write_ && (readiness & POLLOUT));
}
bool ssl_event_socket::poll_queue_ready(short readiness) const {
    if (pending_write_len_ && !pending_write_queued_) return false;
    return queued_bytes() != 0 &&
        (send_need_read_ ? (readiness & POLLIN) : (readiness & POLLOUT));
}
int ssl_event_socket::read(char* buffer, int size) {
    if (!buffer || size <= 0 || !tls_ready_) {
        set_error(ssl_error_io);
        set_transport_error();
        return -1;
    }
    n = poll_receive(buffer, size);
    return n;
}
int ssl_event_socket::read() {
    const int count = read(inbuffer, static_cast<int>(sizeof(inbuffer)));
    if (count > 0) token_size = count;
    return count;
}
int ssl_event_socket::read_line(char* buffer, int size) {
    if (!buffer || size < 1) { set_error(ssl_error_io); return -1; }
    int count = 0;
    while (count < size - 1) {
        char byte{};
        const int received = read(&byte, 1);
        if (received <= 0) {
            buffer[count] = '\0';
            return count ? count : received;
        }
        buffer[count++] = byte;
        if (byte == '\n') break;
    }
    buffer[count] = '\0';
    return count;
}
int ssl_event_socket::read_until(char* buffer, int size) { return read_line(buffer, size); }
std::string ssl_event_socket::read(int size) {
    if (size <= 0) return {};
    std::string result(static_cast<size_t>(size), '\0');
    const int count = read(result.data(), size);
    if (count <= 0) return {};
    result.resize(static_cast<size_t>(count));
    return result;
}
bool ssl_event_socket::poll_error_terminal() const { return true; }
bool ssl_event_socket::poll_has_buffered_input() const {
    return tls_ready_ && session_ && SSL_pending(session_) > 0;
}
} // namespace aeon
#endif
