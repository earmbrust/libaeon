/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/
#include "event_socket.hpp"
#include "internal/callback_state.hpp"
#include "internal/native_poller.hpp"
#include "internal/write_queue.hpp"
#include <cstring>
#include <algorithm>
#include <limits>
#include <utility>

namespace aeon {

event_socket::event_socket() : socket(), callbacks_(new internal::socket_callback_state) {
    if (is_valid_socket() && set_blocking(false) != 0) socket::close();
}

event_socket::event_socket(socket_t fd, bool adopt)
    : socket(fd, adopt), callbacks_(new internal::socket_callback_state) {
    if (is_valid_socket() && set_blocking(false) != 0) socket::close();
}

event_socket::~event_socket() { delete callbacks_; }

void event_socket::on_data(data_handler f) { callbacks_->data.set(std::move(f)); }
void event_socket::on_connect(notification_handler f) { callbacks_->connect.set(std::move(f)); }
void event_socket::on_close(notification_handler f) { callbacks_->close.set(std::move(f)); }
void event_socket::on_listen(notification_handler f) { callbacks_->listen.set(std::move(f)); }
void event_socket::on_error(error_handler f) { callbacks_->error.set(std::move(f)); }
void event_socket::on_drain(notification_handler f) { callbacks_->drain.set(std::move(f)); }

std::size_t event_socket::queued_bytes() const {
    std::lock_guard<std::mutex> guard(callbacks_->outgoing.mutex);
    return callbacks_->outgoing.bytes;
}

bool event_socket::set_max_queued_bytes(std::size_t limit) {
    std::lock_guard<std::mutex> guard(callbacks_->outgoing.mutex);
    if (limit < callbacks_->outgoing.bytes) return false;
    callbacks_->outgoing.limit = limit;
    return true;
}

bool event_socket::queue_write(const std::string& data) {
    return queue_write(data.data(), data.size());
}

bool event_socket::queue_write(const char* data, std::size_t size) {
    if (!is_valid_socket() || !connected || blocking || (size && !data)) return false;
    if (!size) return true;
    auto& out = callbacks_->outgoing;
    std::lock_guard<std::mutex> guard(out.mutex);
    if (size > out.limit - out.bytes) return false;
    // A single large message remains one chunk. Tiny messages share a chunk
    // to prevent a million small queued writes becoming a million allocations.
    constexpr std::size_t coalesce_limit = 16 * 1024;
    if (!out.chunks.empty() && out.chunks.back().size() < coalesce_limit &&
        size <= coalesce_limit - out.chunks.back().size()) {
        out.chunks.back().append(data, size);
    } else {
        out.chunks.emplace_back(data, size);
    }
    out.bytes += size;
    return true;
}

// Called by event_socket_set::poll() after native write readiness. Never
// invokes user code while holding the queue lock.
event_socket::flush_result event_socket::flush_queued(std::size_t budget) {
    if (!is_valid_socket() || blocking || budget == 0) return {};
#if defined(SO_NOSIGPIPE) && !defined(MSG_NOSIGNAL)
    const int no_sigpipe = 1;
    if (::setsockopt(sockfd, SOL_SOCKET, SO_NOSIGPIPE,
        reinterpret_cast<const char*>(&no_sigpipe), sizeof(no_sigpipe)) != 0) {
        const int err = GET_NET_SOCKET_ERROR();
        auto& out = callbacks_->outgoing;
        std::lock_guard<std::mutex> guard(out.mutex);
        out.chunks.clear(); out.front_offset = 0; out.bytes = 0;
        return {0, err, false};
    }
#endif
    const auto progress = internal::flush_pending_writes(callbacks_->outgoing, budget,
        [this](const char* data, int length) -> internal::send_attempt {
            int flags = 0;
#ifdef MSG_NOSIGNAL
            flags |= MSG_NOSIGNAL;
#endif
#ifdef MSG_DONTWAIT
            flags |= MSG_DONTWAIT;
#endif
            const int sent = static_cast<int>(::send(sockfd, data, length, flags));
            return {sent, sent < 0 ? GET_NET_SOCKET_ERROR() : 0};
        });
    return {progress.bytes, progress.error, progress.drained};
}

void event_socket::fire_on_data(const char* data, int size) {
    auto handler = callbacks_->data.snapshot();
    if (handler) (*handler)(data, size);
    else on_data_impl(data, size);
}
void event_socket::fire_on_connect() {
    auto handler = callbacks_->connect.snapshot();
    if (handler) (*handler)();
    else on_connect_impl();
}
void event_socket::fire_on_close() {
    auto handler = callbacks_->close.snapshot();
    if (handler) (*handler)();
    else on_close_impl();
}
void event_socket::fire_on_listen() {
    auto handler = callbacks_->listen.snapshot();
    if (handler) (*handler)();
    else on_listen_impl();
}
void event_socket::fire_on_drain() {
    auto handler = callbacks_->drain.snapshot();
    if (handler) (*handler)();
}
void event_socket::fire_on_error(int error) {
    auto handler = callbacks_->error.snapshot();
    if (handler) (*handler)(error);
    else on_error_impl(error);
}

void event_socket::on_data_impl(const char*, int) {}
void event_socket::on_connect_impl() {}
void event_socket::on_close_impl() {}
void event_socket::on_listen_impl() {}
void event_socket::on_error_impl(int) {}

bool event_socket::close() {
    {
        std::lock_guard<std::mutex> guard(callbacks_->outgoing.mutex);
        auto& out = callbacks_->outgoing;
        out.chunks.clear();
        out.front_offset = 0;
        out.bytes = 0;
    }
    bool closed = socket::close();
    if (closed) fire_on_close();
    return closed;
}

int event_socket::write(const char* data) {
    if (!data || !is_valid_socket()) return NET_SOCKET_ERROR;
    auto len = std::strlen(data);
    if (len == 0) return 0;
    const int sent = static_cast<int>(send(sockfd, data, static_cast<int>(len), 0));
    if (sent < 0) {
        error_code = GET_NET_SOCKET_ERROR();
        fire_on_error(error_code);
    }
    return sent;
}

int event_socket::write(const std::string& data) {
    if (!is_valid_socket() || data.empty()) return NET_SOCKET_ERROR;
    const int sent = static_cast<int>(send(sockfd, data.data(),
                                           static_cast<int>(data.size()), 0));
    if (sent < 0) {
        error_code = GET_NET_SOCKET_ERROR();
        fire_on_error(error_code);
    }
    return sent;
}

} // namespace aeon
