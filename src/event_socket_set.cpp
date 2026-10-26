/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/
#include "aeon.hpp"
#include "event_socket_set.hpp"
#include "server_event_socket.hpp"
#include <chrono>
#include <climits>
#include <thread>
#include <cerrno>
#include <vector>
#include <atomic>
#include <algorithm>
#include <memory>
#include <mutex>
#ifndef NET_PLATFORM_WINDOWS
#include <poll.h>
#endif
#include <utility>

namespace aeon::internal {

// Registrations are non-owning. Shared tokens keep only registration metadata
// alive after removal, not the event_socket itself. Before using the pointer,
// the poller must check get_if_active(). Deregistration is required before
// socket destruction, and destruction must not race an active dispatch.
template<class Socket>
class basic_socket_registration final {
public:
    explicit basic_socket_registration(Socket* socket) noexcept : socket_(socket) {}
    basic_socket_registration(const basic_socket_registration&) = delete;
    basic_socket_registration& operator=(const basic_socket_registration&) = delete;

    Socket* get_if_active() const noexcept {
        return active_.load(std::memory_order_acquire) ? socket_ : nullptr;
    }

private:
    template<class> friend class basic_socket_registry;
    void deactivate() noexcept { active_.store(false, std::memory_order_release); }

    Socket* const socket_;
    std::atomic<bool> active_{true};
};

template<class Socket>
class basic_socket_registry final {
public:
    using token = std::shared_ptr<basic_socket_registration<Socket>>;

    basic_socket_registry() = default;
    basic_socket_registry(const basic_socket_registry&) = delete;
    basic_socket_registry& operator=(const basic_socket_registry&) = delete;
    ~basic_socket_registry() { clear(); }

    bool add(Socket* socket) {
        if (!socket) return false;
        std::lock_guard<std::mutex> lock(mutex_);
        if (find(socket) != entries_.end()) return false;
        entries_.push_back(std::make_shared<basic_socket_registration<Socket>>(socket));
        return true;
    }

    bool remove_index(unsigned int index) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (index >= entries_.size()) return false;
        entries_[index]->deactivate();
        entries_.erase(entries_.begin() + index);
        return true;
    }

    bool remove_socket(Socket* socket) {
        if (!socket) return false;
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = find(socket);
        if (it == entries_.end()) return false;
        (*it)->deactivate();
        entries_.erase(it);
        return true;
    }

    int size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return static_cast<int>(entries_.size());
    }

    // A snapshot remains structurally valid if callbacks remove/readd sockets.
    // Removed tokens become inactive. Not an ownership/lifetime guarantee for
    // the pointed-to socket; that guarantee is the caller's responsibility.
    std::vector<token> snapshot() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return entries_;
    }

    void clear() noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& entry : entries_) entry->deactivate();
        entries_.clear();
    }

private:
    std::vector<token>::iterator find(Socket* socket) {
        return std::find_if(entries_.begin(), entries_.end(),
                            [socket](const token& entry) {
                                return entry->get_if_active() == socket;
                            });
    }

    mutable std::mutex mutex_;
    std::vector<token> entries_;
};

using socket_registry = basic_socket_registry<event_socket>;
using udp_socket_registry = basic_socket_registry<udp_event_socket>;



struct set_poll_state {
    std::mutex poll_mutex;
    socket_registry sockets;
    udp_socket_registry udp_sockets;
};

struct poll_descriptor {
    socket_t fd = invalid_socket;
    short revents = 0;
    bool watch_writable = false;
};

static int socket_pending_error(socket_t fd) {
    int code = 0;
#ifdef NET_PLATFORM_WINDOWS
    int size = sizeof(code);
#else
    socklen_t size = sizeof(code);
#endif
    if (::getsockopt(fd, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&code), &size) != 0)
        return GET_NET_SOCKET_ERROR();
    return code;
}

static int poll_readable(std::vector<poll_descriptor>& descriptors, int timeout_ms) {
    if (timeout_ms < -1) return -1;
    // revents belongs to this invocation, even on timeout or native error.
    // Callers may reuse descriptors for successive readiness checks.
    for (auto& entry : descriptors) entry.revents = 0;
    if (descriptors.empty()) {
        if (timeout_ms > 0) std::this_thread::sleep_for(std::chrono::milliseconds(timeout_ms));
        // An empty set never waits indefinitely.
        return 0;
    }
#ifdef NET_PLATFORM_WINDOWS
    if (descriptors.size() > static_cast<size_t>(ULONG_MAX)) return -1;
    std::vector<WSAPOLLFD> native;
    native.reserve(descriptors.size());
    for (const auto& entry : descriptors) native.push_back({entry.fd, static_cast<SHORT>(POLLRDNORM | (entry.watch_writable ? POLLWRNORM : 0)), 0});
    const int result = ::WSAPoll(native.data(), static_cast<ULONG>(native.size()), timeout_ms);
#else
    std::vector<pollfd> native;
    native.reserve(descriptors.size());
    for (const auto& entry : descriptors) native.push_back({entry.fd, static_cast<short>(POLLIN | (entry.watch_writable ? POLLOUT : 0)), 0});
    int result = -1;
    if (timeout_ms == -1) {
        do { result = ::poll(native.data(), static_cast<nfds_t>(native.size()), -1); }
        while (result < 0 && errno == EINTR);
    } else {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
        int remaining = timeout_ms;
        do {
            result = ::poll(native.data(), static_cast<nfds_t>(native.size()), remaining);
            if (result >= 0 || errno != EINTR) break;
            const auto now = std::chrono::steady_clock::now();
            if (now >= deadline) return 0;
            remaining = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count());
        } while (true);
    }
#endif
    if (result <= 0) return result;
    for (size_t i = 0; i < native.size(); ++i) descriptors[i].revents = native[i].revents;
    return result;
}
} // namespace aeon::internal

namespace aeon {
enum { handler_data, handler_connect, handler_close, handler_listen, handler_error, handler_drain, handler_udp_datagram, handler_udp_error };

event_socket_set::event_socket_set() : internal::event_handlers(8), state_(new internal::set_poll_state) {}
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

void event_socket_set::on_socket_data(data_handler f) { set_handler(handler_data, f ? std::make_shared<const data_handler>(std::move(f)) : std::shared_ptr<const void>{}); }
void event_socket_set::on_socket_connect(notification_handler f) { set_handler(handler_connect, f ? std::make_shared<const notification_handler>(std::move(f)) : std::shared_ptr<const void>{}); }
void event_socket_set::on_socket_close(notification_handler f) { set_handler(handler_close, f ? std::make_shared<const notification_handler>(std::move(f)) : std::shared_ptr<const void>{}); }
void event_socket_set::on_socket_listen(notification_handler f) { set_handler(handler_listen, f ? std::make_shared<const notification_handler>(std::move(f)) : std::shared_ptr<const void>{}); }
void event_socket_set::on_socket_error(error_handler f) { set_handler(handler_error, f ? std::make_shared<const error_handler>(std::move(f)) : std::shared_ptr<const void>{}); }
void event_socket_set::on_socket_drain(notification_handler f) { set_handler(handler_drain, f ? std::make_shared<const notification_handler>(std::move(f)) : std::shared_ptr<const void>{}); }
void event_socket_set::on_socket_datagram(datagram_handler f) { set_handler(handler_udp_datagram, f ? std::make_shared<const datagram_handler>(std::move(f)) : std::shared_ptr<const void>{}); }
void event_socket_set::on_udp_socket_error(udp_error_handler f) { set_handler(handler_udp_error, f ? std::make_shared<const udp_error_handler>(std::move(f)) : std::shared_ptr<const void>{}); }

void event_socket_set::fire_on_socket_data(event_socket* s, const char* d, int n) {
    auto f = std::static_pointer_cast<const data_handler>(get_handler(handler_data));
    if (f) (*f)(s, d, n); else on_socket_data_impl(s, d, n);
}
void event_socket_set::fire_on_socket_connect(event_socket* s) {
    auto f = std::static_pointer_cast<const notification_handler>(get_handler(handler_connect));
    if (f) (*f)(s); else on_socket_connect_impl(s);
}
void event_socket_set::fire_on_socket_close(event_socket* s) {
    auto f = std::static_pointer_cast<const notification_handler>(get_handler(handler_close));
    if (f) (*f)(s); else on_socket_close_impl(s);
}
void event_socket_set::fire_on_socket_listen(event_socket* s) {
    auto f = std::static_pointer_cast<const notification_handler>(get_handler(handler_listen));
    if (f) (*f)(s); else on_socket_listen_impl(s);
}
void event_socket_set::fire_on_socket_drain(event_socket* s) {
    auto f = std::static_pointer_cast<const notification_handler>(get_handler(handler_drain));
    if (f) (*f)(s); else on_socket_drain_impl(s);
}
void event_socket_set::fire_on_socket_error(event_socket* s, int code) {
    auto f = std::static_pointer_cast<const error_handler>(get_handler(handler_error));
    if (f) (*f)(s, code); else on_socket_error_impl(s, code);
}
void event_socket_set::fire_on_socket_datagram(udp_event_socket* s, const char* d, int n, const address& a) {
    auto f = std::static_pointer_cast<const datagram_handler>(get_handler(handler_udp_datagram));
    if (f) (*f)(s, d, n, a); else on_socket_datagram_impl(s, d, n, a);
}
void event_socket_set::fire_on_udp_socket_error(udp_event_socket* s, int c) {
    auto f = std::static_pointer_cast<const udp_error_handler>(get_handler(handler_udp_error));
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
        native.push_back({s->sockfd, 0, s->poll_wants_write()});
    }
    for (const auto& entry : udp_snapshot) {
        auto* s = entry->get_if_active();
        if (!s || !s->is_valid_socket()) continue;
        watched.push_back({{}, entry, s->sockfd});
        native.push_back({s->sockfd, 0});
    }
    // OpenSSL may already have decrypted bytes in its record buffer even when
    // the underlying descriptor has no new readable data.
    bool buffered = false;
    for (size_t i = 0; i < watched.size(); ++i) {
        if (watched[i].tcp) {
            auto* s = watched[i].tcp->get_if_active();
            if (s && s->is_valid_socket() && s->sockfd == watched[i].fd &&
                s->poll_has_buffered_input()) buffered = true;
        }
    }
    // Bound even an infinite poll by the nearest active transport deadline.
    // This is essential for silent TLS peers that never send a ClientHello.
    int effective_timeout = timeout_ms;
    for (const auto& watch : watched) {
        if (!watch.tcp) continue;
        auto* s = watch.tcp->get_if_active();
        if (!s || !s->is_valid_socket() || s->sockfd != watch.fd) continue;
        const int due = s->poll_timeout_ms();
        if (due >= 0 && (effective_timeout == -1 || due < effective_timeout))
            effective_timeout = due;
    }
    const int ready = internal::poll_readable(native, buffered ? 0 : effective_timeout);
    if (ready < 0) return ready;
    if (buffered) {
        for (size_t i = 0; i < watched.size(); ++i) {
            if (watched[i].tcp) {
                auto* s = watched[i].tcp->get_if_active();
                if (s && s->is_valid_socket() && s->sockfd == watched[i].fd &&
                    s->poll_has_buffered_input()) native[i].revents |= POLLIN;
            }
        }
    }
    bool expired = false;
    for (const auto& watch : watched) {
        if (!watch.tcp) continue;
        auto* s = watch.tcp->get_if_active();
        if (s && s->is_valid_socket() && s->sockfd == watch.fd &&
            s->poll_timeout_ms() == 0) expired = true;
    }
    if (ready == 0 && !buffered && !expired) return 0;
    int handled = 0;
    char buffer[socket::max_buffer_size];
    for (size_t i = 0; i < watched.size(); ++i) {
        const auto flags = native[i].revents;
        const auto& watch = watched[i];
        // A handshake can time out without any native readiness event.
        if (!flags && !(watch.tcp && watch.tcp->get_if_active() &&
                        watch.tcp->get_if_active()->poll_timeout_ms() == 0)) continue;
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
            // TLS handshakes must finish before application I/O is dispatched.
            const int progress = s->poll_step(flags);
            if (progress <= 0) {
                ++handled;
                if (progress < 0) {
                    const int err = s->get_error();
                    s->n = NET_SOCKET_ERROR;
                    s->fire_on_error(err);
                    if (watch.tcp->get_if_active()) fire_on_socket_error(s, err);
                    if (watch.tcp->get_if_active() && s->is_valid_socket()) s->close();
                }
                continue;
            }
            if (progress == 2) {
                ++handled;
                if (watch.tcp->get_if_active() && s->is_valid_socket() &&
                    s->sockfd == watch.fd) fire_on_socket_connect(s);
                continue;
            }
            if (s->poll_read_ready(flags)) {
                const int bytes = s->poll_receive(buffer, static_cast<int>(sizeof(buffer)));
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
                        if (watch.tcp->get_if_active() && s->is_valid_socket() &&
                            s->sockfd == watch.fd && s->poll_error_terminal()) s->close();
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
            if (watch.tcp->get_if_active() && s->is_valid_socket() &&
                s->sockfd == watch.fd && s->poll_queue_ready(flags)) {
                constexpr std::size_t send_budget = 64 * 1024;
                const auto flushed = s->flush_queued(send_budget);
                if (flushed.bytes || flushed.drained || flushed.error) ++handled;
                if (flushed.error) {
                    s->set_error(flushed.error);
                    s->n = NET_SOCKET_ERROR;
                    s->fire_on_error(flushed.error);
                    if (watch.tcp->get_if_active()) fire_on_socket_error(s, flushed.error);
                    if (watch.tcp->get_if_active() && s->is_valid_socket() &&
                        s->sockfd == watch.fd && s->poll_error_terminal()) s->close();
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
