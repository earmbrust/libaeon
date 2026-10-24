#pragma once

#include "aeon_export.hpp"
#include <cstddef>
#include <memory>

namespace aeon::internal {

// Shared callback registration and dispatch state. Handler types are kept in
// their corresponding event classes; the base owns synchronized snapshots.
class LIBAEON_API event_handlers {
protected:
    explicit event_handlers(std::size_t count);
    ~event_handlers();
    event_handlers(const event_handlers&) = delete;
    event_handlers& operator=(const event_handlers&) = delete;

    void set_handler(std::size_t slot, std::shared_ptr<const void> handler);
    std::shared_ptr<const void> get_handler(std::size_t slot) const;

private:
    struct state;
    state* callbacks_;
};

} // namespace aeon::internal
