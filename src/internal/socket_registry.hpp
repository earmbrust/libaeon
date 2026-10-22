#pragma once

#include <algorithm>
#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

namespace aeon { class event_socket; class udp_event_socket; }

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

} // namespace aeon::internal
