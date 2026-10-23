#pragma once

#include "callback_state.hpp"
#include "internal/config.hpp"
#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <limits>
#include <mutex>

namespace aeon::internal {

// The error is captured immediately by the caller of the native send function.
// Synthetic attempts let tests exercise OS-dependent short-send paths without
// manipulating socket buffer sizes or depending on platform scheduling.
struct send_attempt {
    int bytes = -1;
    int error = 0;
};

struct send_progress {
    std::size_t bytes = 0;
    int error = 0;
    bool drained = false;
};

inline bool interrupted_send_error(int error) noexcept {
#ifdef NET_PLATFORM_WINDOWS
    return error == WSAEINTR;
#else
    return error == EINTR;
#endif
}

// Caller's send operation MUST be nonblocking and must not retain the buffer.
// Never runs user callbacks. Queue mutations are serialized by the queue lock,
// including throughout the native send call. No allocation on the flush path.
template<class Sender>
send_progress flush_pending_writes(pending_write_state& out, std::size_t budget,
                                   Sender&& sender) {
    send_progress result{};
    if (!budget) return result;
    std::lock_guard<std::mutex> guard(out.mutex);
    const bool was_nonempty = out.bytes != 0;
    unsigned int interrupts = 0;
    while (out.bytes && result.bytes < budget) {
        const auto& front = out.chunks.front();
        const auto available = front.size() - out.front_offset;
        const auto requested = static_cast<int>(std::min({available, budget - result.bytes,
            static_cast<std::size_t>(std::numeric_limits<int>::max())}));
        const auto attempt = sender(front.data() + out.front_offset, requested);
        if (attempt.bytes < 0) {
            if (interrupted_send_error(attempt.error) && ++interrupts <= 4) continue;
            if (interrupted_send_error(attempt.error) || would_block_error(attempt.error)) break;
            result.error = attempt.error ? attempt.error : invalid_descriptor_error();
            out.chunks.clear();
            out.front_offset = 0;
            out.bytes = 0;
            break;
        }
        if (attempt.bytes == 0) break; // No progress: do not spin.
        if (attempt.bytes > requested) {
            result.error = invalid_descriptor_error(); // Violated native-send contract.
            out.chunks.clear();
            out.front_offset = 0;
            out.bytes = 0;
            break;
        }
        interrupts = 0;
        const auto count = static_cast<std::size_t>(attempt.bytes);
        out.front_offset += count;
        out.bytes -= count;
        result.bytes += count;
        if (out.front_offset == front.size()) {
            out.chunks.pop_front();
            out.front_offset = 0;
        }
    }
    result.drained = was_nonempty && !out.bytes && !result.error;
    return result;
}

} // namespace aeon::internal
