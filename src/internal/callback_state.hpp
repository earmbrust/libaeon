#pragma once

#include "address.hpp"
#include "socket.hpp"
#include "socket_registry.hpp"

namespace aeon { class event_socket; class udp_event_socket; }

#include <functional>
#include <memory>
#include <mutex>
#include <utility>

namespace aeon::internal {

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

struct socket_callback_state {
    callback_slot<void(const char*, int)> data;
    callback_slot<void(const char*, int, const address&)> datagram;
    callback_slot<void()> connect;
    callback_slot<void()> close;
    callback_slot<void()> listen;
    callback_slot<void(int)> error;
};

struct set_callback_state {
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
