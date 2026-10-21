/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/
#include "aeon.hpp"
#include "event_socket.hpp"
#include <cerrno>
#include <deque>
#include <mutex>
#include <vector>
#include <string>
#include <cstring>
#include <algorithm>
#include <limits>
#include <memory>
#include <utility>

namespace aeon::internal {

struct event_handlers::state {
    explicit state(std::size_t count) : handlers(count) {}
    mutable std::mutex mutex;
    std::vector<std::shared_ptr<const void>> handlers;
};

event_handlers::event_handlers(std::size_t count) : callbacks_(new state(count)) {}
event_handlers::~event_handlers() { delete callbacks_; }

void event_handlers::set_handler(std::size_t slot, std::shared_ptr<const void> handler) {
    std::lock_guard<std::mutex> lock(callbacks_->mutex);
    callbacks_->handlers.at(slot) = std::move(handler);
}

std::shared_ptr<const void> event_handlers::get_handler(std::size_t slot) const {
    std::lock_guard<std::mutex> lock(callbacks_->mutex);
    return callbacks_->handlers.at(slot);
}

struct pending_write_state {
    mutable std::mutex mutex;
    std::deque<std::string> chunks;
    std::size_t front_offset = 0;
    std::size_t bytes = 0;
    std::size_t limit = 1024 * 1024;
};



} // namespace aeon::internal

namespace aeon {
enum { handler_data, handler_connect, handler_close, handler_listen, handler_error, handler_drain };


event_socket::event_socket() : socket(), internal::event_handlers(6), outgoing_(new internal::pending_write_state) {
    if (is_valid_socket() && set_blocking(false) != 0) socket::close();
}

event_socket::event_socket(socket_t fd, bool adopt)
    : socket(fd, adopt), internal::event_handlers(6), outgoing_(new internal::pending_write_state) {
    if (is_valid_socket() && set_blocking(false) != 0) socket::close();
}

event_socket::~event_socket() { delete outgoing_; }

void event_socket::on_data(data_handler f) { set_handler(handler_data, f ? std::make_shared<const data_handler>(std::move(f)) : std::shared_ptr<const void>{}); }
void event_socket::on_connect(notification_handler f) { set_handler(handler_connect, f ? std::make_shared<const notification_handler>(std::move(f)) : std::shared_ptr<const void>{}); }
void event_socket::on_close(notification_handler f) { set_handler(handler_close, f ? std::make_shared<const notification_handler>(std::move(f)) : std::shared_ptr<const void>{}); }
void event_socket::on_listen(notification_handler f) { set_handler(handler_listen, f ? std::make_shared<const notification_handler>(std::move(f)) : std::shared_ptr<const void>{}); }
void event_socket::on_error(error_handler f) { set_handler(handler_error, f ? std::make_shared<const error_handler>(std::move(f)) : std::shared_ptr<const void>{}); }
void event_socket::on_drain(notification_handler f) { set_handler(handler_drain, f ? std::make_shared<const notification_handler>(std::move(f)) : std::shared_ptr<const void>{}); }

std::size_t event_socket::queued_bytes() const {
    std::lock_guard<std::mutex> guard((*outgoing_).mutex);
    return (*outgoing_).bytes;
}

bool event_socket::set_max_queued_bytes(std::size_t limit) {
    std::lock_guard<std::mutex> guard((*outgoing_).mutex);
    if (limit < (*outgoing_).bytes) return false;
    (*outgoing_).limit = limit;
    return true;
}

bool event_socket::queue_write(const std::string& data) {
    return queue_write(data.data(), data.size());
}

bool event_socket::queue_write(const char* data, std::size_t size) {
    if (!is_valid_socket() || !connected || blocking || (size && !data)) return false;
    if (!size) return true;
    auto& out = (*outgoing_);
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
        auto& out = (*outgoing_);
        std::lock_guard<std::mutex> guard(out.mutex);
        out.chunks.clear(); out.front_offset = 0; out.bytes = 0;
        return {0, err, false};
    }
#endif
    auto& out = (*outgoing_);
    std::lock_guard<std::mutex> guard(out.mutex);
    const bool was_nonempty = out.bytes != 0;
    flush_result result{};
    unsigned int interrupts = 0;
    while (out.bytes && result.bytes < budget) {
        const auto& front = out.chunks.front();
        const auto available = front.size() - out.front_offset;
        const int requested = static_cast<int>(std::min({available, budget - result.bytes,
            static_cast<std::size_t>(std::numeric_limits<int>::max())}));
        int flags = 0;
#ifdef MSG_NOSIGNAL
        flags |= MSG_NOSIGNAL;
#endif
#ifdef MSG_DONTWAIT
        flags |= MSG_DONTWAIT;
#endif
        const int sent = static_cast<int>(::send(sockfd, front.data() + out.front_offset,
                                                  requested, flags));
        const int error = sent < 0 ? GET_NET_SOCKET_ERROR() : 0;
        if (sent < 0) {
#ifdef NET_PLATFORM_WINDOWS
            const bool interrupted = error == WSAEINTR;
#else
            const bool interrupted = error == EINTR;
#endif
            if (interrupted && ++interrupts <= 4) continue;
            if (interrupted || internal::would_block_error(error)) break;
            result.error = error ? error : internal::invalid_descriptor_error();
            out.chunks.clear();
            out.front_offset = 0;
            out.bytes = 0;
            break;
        }
        if (sent == 0) break;
        if (sent > requested) {
            result.error = internal::invalid_descriptor_error();
            out.chunks.clear();
            out.front_offset = 0;
            out.bytes = 0;
            break;
        }
        interrupts = 0;
        const auto count = static_cast<std::size_t>(sent);
        out.front_offset += count;
        out.bytes -= count;
        result.bytes += count;
        if (out.front_offset == front.size()) {
            out.chunks.pop_front();
            out.front_offset = 0;
        }
    }
    result.drained = was_nonempty && !out.bytes && !result.error;
    return result;
}

void event_socket::fire_on_data(const char* data, int size) {
    auto handler = std::static_pointer_cast<const data_handler>(get_handler(handler_data));
    if (handler) (*handler)(data, size);
    else on_data_impl(data, size);
}
void event_socket::fire_on_connect() {
    auto handler = std::static_pointer_cast<const notification_handler>(get_handler(handler_connect));
    if (handler) (*handler)();
    else on_connect_impl();
}
void event_socket::fire_on_close() {
    auto handler = std::static_pointer_cast<const notification_handler>(get_handler(handler_close));
    if (handler) (*handler)();
    else on_close_impl();
}
void event_socket::fire_on_listen() {
    auto handler = std::static_pointer_cast<const notification_handler>(get_handler(handler_listen));
    if (handler) (*handler)();
    else on_listen_impl();
}
void event_socket::fire_on_drain() {
    auto handler = std::static_pointer_cast<const notification_handler>(get_handler(handler_drain));
    if (handler) (*handler)();
}
void event_socket::fire_on_error(int error) {
    auto handler = std::static_pointer_cast<const error_handler>(get_handler(handler_error));
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
        std::lock_guard<std::mutex> guard((*outgoing_).mutex);
        auto& out = (*outgoing_);
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
