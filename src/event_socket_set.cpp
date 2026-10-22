/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/
#include "event_socket_set.hpp"
#include "internal/callback_state.hpp"
#include "internal/native_poller.hpp"
#include "server_event_socket.hpp"
#include <vector>
#include <mutex>
#ifndef NET_PLATFORM_WINDOWS
#include <poll.h>
#endif
#include <utility>

namespace aeon {
event_socket_set::event_socket_set() : state_(new internal::set_callback_state) {}
event_socket_set::~event_socket_set() { delete state_; }

bool event_socket_set::add(event_socket* socket) {
    return state_->sockets.add(socket);
}
bool event_socket_set::remove(unsigned int index) {
    const int tcp_count = state_->sockets.size();
    if (index < static_cast<unsigned int>(tcp_count)) return state_->sockets.remove_index(index);
    return state_->udp_sockets.remove_index(index - static_cast<unsigned int>(tcp_count));
}
bool event_socket_set::add(udp_event_socket* socket) {
    return state_->udp_sockets.add(socket);
}
bool event_socket_set::remove_udp_socket(udp_event_socket* socket) {
    return state_->udp_sockets.remove_socket(socket);
}
bool event_socket_set::remove_socket(event_socket* socket) {
    return state_->sockets.remove_socket(socket);
}
int event_socket_set::size() const {
    return state_->sockets.size() + state_->udp_sockets.size();
}

void event_socket_set::on_socket_data(data_handler f) { state_->data.set(std::move(f)); }
void event_socket_set::on_socket_connect(notification_handler f) { state_->connect.set(std::move(f)); }
void event_socket_set::on_socket_close(notification_handler f) { state_->close.set(std::move(f)); }
void event_socket_set::on_socket_listen(notification_handler f) { state_->listen.set(std::move(f)); }
void event_socket_set::on_socket_error(error_handler f) { state_->error.set(std::move(f)); }
void event_socket_set::on_socket_drain(notification_handler f) { state_->drain.set(std::move(f)); }
void event_socket_set::on_socket_datagram(datagram_handler f) { state_->udp_datagram.set(std::move(f)); }
void event_socket_set::on_udp_socket_error(udp_error_handler f) { state_->udp_error.set(std::move(f)); }

void event_socket_set::fire_on_socket_data(event_socket* s, const char* d, int n) {
    auto f = state_->data.snapshot();
    if (f) (*f)(s, d, n); else on_socket_data_impl(s, d, n);
}
void event_socket_set::fire_on_socket_connect(event_socket* s) {
    auto f = state_->connect.snapshot();
    if (f) (*f)(s); else on_socket_connect_impl(s);
}
void event_socket_set::fire_on_socket_close(event_socket* s) {
    auto f = state_->close.snapshot();
    if (f) (*f)(s); else on_socket_close_impl(s);
}
void event_socket_set::fire_on_socket_listen(event_socket* s) {
    auto f = state_->listen.snapshot();
    if (f) (*f)(s); else on_socket_listen_impl(s);
}
void event_socket_set::fire_on_socket_drain(event_socket* s) {
    auto f = state_->drain.snapshot();
    if (f) (*f)(s); else on_socket_drain_impl(s);
}
void event_socket_set::fire_on_socket_error(event_socket* s, int code) {
    auto f = state_->error.snapshot();
    if (f) (*f)(s, code); else on_socket_error_impl(s, code);
}
void event_socket_set::fire_on_socket_datagram(udp_event_socket* s, const char* d, int n, const address& a) {
    auto f = state_->udp_datagram.snapshot();
    if (f) (*f)(s, d, n, a); else on_socket_datagram_impl(s, d, n, a);
}
void event_socket_set::fire_on_udp_socket_error(udp_event_socket* s, int c) {
    auto f = state_->udp_error.snapshot();
    if (f) (*f)(s, c); else on_udp_socket_error_impl(s, c);
}

// Serializes pollers without blocking callback-driven add/remove. A reentrant
// call to poll returns -1; this mutex is never used by registration methods.
int event_socket_set::poll(int timeout_ms) {
    if (timeout_ms < -1) return -1;
    std::unique_lock<std::mutex> single_poll(state_->poll_mutex, std::try_to_lock);
    if (!single_poll.owns_lock()) return -1;
    struct pending {
        internal::socket_registry::token tcp;
        internal::udp_socket_registry::token udp;
        socket_t fd;
    };
    std::vector<pending> watched;
    std::vector<internal::poll_descriptor> native;
    const auto tcp_snapshot = state_->sockets.snapshot();
    const auto udp_snapshot = state_->udp_sockets.snapshot();
    watched.reserve(tcp_snapshot.size() + udp_snapshot.size());
    native.reserve(watched.capacity());
    for (const auto& entry : tcp_snapshot) {
        auto* s = entry->get_if_active();
        if (!s || !s->is_valid_socket()) continue;
        watched.push_back({entry, {}, s->sockfd});
        native.push_back({s->sockfd, 0, s->queued_bytes() != 0});
    }
    for (const auto& entry : udp_snapshot) {
        auto* s = entry->get_if_active();
        if (!s || !s->is_valid_socket()) continue;
        watched.push_back({{}, entry, s->sockfd});
        native.push_back({s->sockfd, 0});
    }
    const int ready = internal::poll_readable(native, timeout_ms);
    if (ready <= 0) return ready;
    int handled = 0;
    char buffer[socket::max_buffer_size];
    for (size_t i = 0; i < watched.size(); ++i) {
        const auto flags = native[i].revents;
        if (!flags) continue;
        const auto& watch = watched[i];
        if (watch.tcp) {
            auto* s = watch.tcp->get_if_active();
            if (!s || !s->is_valid_socket() || s->sockfd != watch.fd) continue;
            // A listening TCP socket signals accept readiness; the application
            // accepts and registers each client, retaining ownership itself.
            if (dynamic_cast<server_event_socket*>(s)) {
                if (flags & POLLIN) {
                    ++handled;
                    fire_on_socket_listen(s);
                } else if (flags & (POLLERR | POLLNVAL | POLLHUP)) {
                    const int err = (flags & POLLNVAL) ? internal::invalid_descriptor_error()
                        : internal::socket_pending_error(watch.fd);
                    ++handled;
                    s->set_error(err);
                    s->n = NET_SOCKET_ERROR;
                    s->fire_on_error(err);
                    if (watch.tcp->get_if_active()) fire_on_socket_error(s, err);
                }
                continue;
            }
            if (flags & (POLLIN | POLLHUP)) {
                const int bytes = static_cast<int>(::recv(watch.fd, buffer, sizeof(buffer), 0));
                if (bytes > 0) {
                    ++handled;
                    s->n = bytes;
                    s->fire_on_data(buffer, bytes);
                    if (watch.tcp->get_if_active()) fire_on_socket_data(s, buffer, bytes);
                } else if (bytes == 0) {
                    ++handled;
                    s->close();
                    if (watch.tcp->get_if_active()) fire_on_socket_close(s);
                } else {
                    const int err = GET_NET_SOCKET_ERROR();
                    if (!internal::would_block_error(err)) {
                        ++handled;
                        s->set_error(err);
                        s->n = NET_SOCKET_ERROR;
                        s->fire_on_error(err);
                        if (watch.tcp->get_if_active()) fire_on_socket_error(s, err);
                    }
                }
            } else if (flags & (POLLERR | POLLNVAL)) {
                const int err = (flags & POLLNVAL) ? internal::invalid_descriptor_error()
                    : internal::socket_pending_error(watch.fd);
                ++handled;
                s->set_error(err);
                s->n = NET_SOCKET_ERROR;
                s->fire_on_error(err);
                if (watch.tcp->get_if_active()) fire_on_socket_error(s, err);
            }
            // Check registration and descriptor again: the read/error handler
            // may have closed, removed or replaced this socket.
            if ((flags & POLLOUT) && watch.tcp->get_if_active() &&
                s->is_valid_socket() && s->sockfd == watch.fd) {
                constexpr std::size_t send_budget = 64 * 1024;
                const auto flushed = s->flush_queued(send_budget);
                if (flushed.bytes || flushed.drained || flushed.error) ++handled;
                if (flushed.error) {
                    s->set_error(flushed.error);
                    s->n = NET_SOCKET_ERROR;
                    s->fire_on_error(flushed.error);
                    if (watch.tcp->get_if_active()) fire_on_socket_error(s, flushed.error);
                } else if (flushed.drained) {
                    s->fire_on_drain();
                    if (watch.tcp->get_if_active()) fire_on_socket_drain(s);
                }
            }
        } else {
            auto* s = watch.udp->get_if_active();
            if (!s || !s->is_valid_socket() || s->sockfd != watch.fd) continue;
            if (flags & POLLIN) {
                sockaddr_storage peer{};
                socklen_t length = static_cast<socklen_t>(sizeof(peer));
                const int bytes = static_cast<int>(::recvfrom(watch.fd, buffer, sizeof(buffer), 0,
                    reinterpret_cast<sockaddr*>(&peer), &length));
                if (bytes >= 0) { // A zero-byte UDP datagram is valid.
                    ++handled;
                    s->n = bytes;
                    s->remote_addr = peer; // Reply address, consistent with udp_socket::read.
                    const address sender(peer);
                    s->fire_on_datagram(buffer, bytes, sender);
                    if (watch.udp->get_if_active()) fire_on_socket_datagram(s, buffer, bytes, sender);
                } else {
                    const int err = GET_NET_SOCKET_ERROR();
                    if (!internal::would_block_error(err)) {
                        ++handled;
                        s->set_error(err);
                        s->n = NET_SOCKET_ERROR;
                        s->fire_on_error(err);
                        if (watch.udp->get_if_active()) fire_on_udp_socket_error(s, err);
                    }
                }
            } else if (flags & (POLLERR | POLLNVAL)) {
                const int err = (flags & POLLNVAL) ? internal::invalid_descriptor_error()
                    : internal::socket_pending_error(watch.fd);
                ++handled;
                s->set_error(err);
                s->n = NET_SOCKET_ERROR;
                s->fire_on_error(err);
                if (watch.udp->get_if_active()) fire_on_udp_socket_error(s, err);
            }
        }
    }
    return handled;
}
} // namespace aeon
