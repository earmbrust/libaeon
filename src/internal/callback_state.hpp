#pragma once

#include "address.hpp"
#include "socket.hpp"

namespace aeon { class event_socket; class udp_event_socket; }

#include <atomic>
#include <algorithm>
#include <vector>
#include <functional>
#include <deque>
#include <string>
#include <cstddef>
#include <memory>
#include <mutex>
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



// Replacements allocate only at registration. Dispatch snapshots a shared
// handler without copying std::function or holding a lock across user code.
template <typename Signature>
class callback_slot {
public:
    using handler = std::function<Signature>;

    void set(handler next) {
        auto prepared = next ? std::make_shared<const handler>(std::move(next))
                             : std::shared_ptr<const handler>{};
        std::lock_guard<std::mutex> lock(mutex_);
        callback_ = std::move(prepared);
    }

    std::shared_ptr<const handler> snapshot() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return callback_;
    }

    callback_slot(const callback_slot&) = delete;
    callback_slot& operator=(const callback_slot&) = delete;
    callback_slot() = default;

private:
    mutable std::mutex mutex_;
    std::shared_ptr<const handler> callback_;
};

// Only event_socket uses this queue. Producers submit complete writes while
// the application's poller flushes them in bounded nonblocking sends.
struct pending_write_state {
    mutable std::mutex mutex;
    std::deque<std::string> chunks;
    std::size_t front_offset = 0;
    std::size_t bytes = 0;
    std::size_t limit = 1024 * 1024;
};

struct socket_callback_state {
    pending_write_state outgoing;
    callback_slot<void()> drain;
    callback_slot<void(const char*, int)> data;
    callback_slot<void(const char*, int, const address&)> datagram;
    callback_slot<void()> connect;
    callback_slot<void()> close;
    callback_slot<void()> listen;
    callback_slot<void(int)> error;
};

struct set_callback_state {
    callback_slot<void(event_socket*)> drain;
    std::mutex poll_mutex;
    callback_slot<void(event_socket*, const char*, int)> data;
    callback_slot<void(event_socket*)> connect;
    callback_slot<void(event_socket*)> close;
    callback_slot<void(event_socket*)> listen;
    callback_slot<void(event_socket*, int)> error;

    // Non-owning registry with invalidatable poll snapshots.
    socket_registry sockets;
    udp_socket_registry udp_sockets;
    callback_slot<void(udp_event_socket*, const char*, int, const address&)> udp_datagram;
    callback_slot<void(udp_event_socket*, int)> udp_error;
};

} // namespace aeon::internal
