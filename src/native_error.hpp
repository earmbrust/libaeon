#pragma once
#include "aeon.hpp"

namespace aeon {

LIBAEON_API int translate_native_error(int native_error) noexcept;

inline int invalid_descriptor_error() noexcept {
#ifdef NET_PLATFORM_WINDOWS
    return WSAENOTSOCK;
#else
    return EBADF;
#endif
}

inline bool would_block_error(int error) noexcept {
#ifdef NET_PLATFORM_WINDOWS
    return error == WSAEWOULDBLOCK;
#else
    return error == EAGAIN || error == EWOULDBLOCK || error == EINTR;
#endif
}

} // namespace aeon
