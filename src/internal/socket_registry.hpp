#pragma once

#include <algorithm>
#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

namespace aeon { class event_socket; }

namespace aeon::internal {

// Registrations are non-owning. Shared tokens keep only registration metadata
// alive after removal, not the event_socket itself. Before using the pointer,
// the poller must check get_if_active(). Deregistration is required before
// socket destruction, and destruction must not race an active dispatch.
class socket_registration final {
public:
    explicit socket_registration(event_socket* socket) noexcept : socket_(socket) {}
    socket_registration(const socket_registration&) = delete;
    socket_registration& operator=(const socket_registration&) = delete;

    event_socket* get_if_active() const noexcept {
        return active_.load(std::memory_order_acquire) ? socket_ : nullptr;
    }

private:
    friend class socket_registry;
    void deactivate() noexcept { active_.store(false, std::memory_order_release); }

    event_socket* const socket_;
    std::atomic<bool> active_{true};
};

class socket_registry final {
public:
    using token = std::shared_ptr<socket_registration>;

    socket_registry() = default;
    socket_registry(const socket_registry&) = delete;
    socket_registry& operator=(const socket_registry&) = delete;
    ~socket_registry() { clear(); }

    bool add(event_socket* socket) {
        if (!socket) return false;
        std::lock_guard<std::mutex> lock(mutex_);
        if (find(socket) != entries_.end()) return false;
        entries_.push_back(std::make_shared<socket_registration>(socket));
        return true;
    }

    bool remove_index(unsigned int index) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (index >= entries_.size()) return false;
        entries_[index]->deactivate();
        entries_.erase(entries_.begin() + index);
        return true;
    }

    bool remove_socket(event_socket* socket) {
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
    std::vector<token>::iterator find(event_socket* socket) {
        return std::find_if(entries_.begin(), entries_.end(),
                            [socket](const token& entry) {
                                return entry->get_if_active() == socket;
                            });
    }

    mutable std::mutex mutex_;
    std::vector<token> entries_;
};

} // namespace aeon::internal
