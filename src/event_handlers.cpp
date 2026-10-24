#include "aeon.hpp"
#include "internal/event_handlers.hpp"
#include <mutex>
#include <utility>
#include <vector>

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

} // namespace aeon::internal
