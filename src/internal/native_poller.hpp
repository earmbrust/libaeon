#pragma once
#include "internal/config.hpp"
#include <vector>

namespace aeon::internal {
struct poll_descriptor {
    socket_t fd = invalid_socket;
    short revents = 0;
    bool watch_writable = false;
};

// All network sockets are level-triggered for read/error readiness;
// optionally watch write readiness for sockets with queued outgoing data.
// Does not own descriptors, create threads or invoke callbacks.
// A valid poll invocation clears all previous revents, including on timeout.
int poll_readable(std::vector<poll_descriptor>& descriptors, int timeout_ms);
int socket_pending_error(socket_t fd);
int invalid_descriptor_error() noexcept;
int would_block_error(int error) noexcept;
}
