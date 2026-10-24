#pragma once

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

} // namespace aeon::internal
