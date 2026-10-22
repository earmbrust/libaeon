#include "native_poller.hpp"
#include <chrono>
#include <climits>
#include <thread>
#include <vector>
#ifndef NET_PLATFORM_WINDOWS
#include <poll.h>
#include <cerrno>
#endif

namespace aeon::internal {

int invalid_descriptor_error() noexcept {
#ifdef NET_PLATFORM_WINDOWS
    return WSAENOTSOCK;
#else
    return EBADF;
#endif
}

int would_block_error(int error) noexcept {
#ifdef NET_PLATFORM_WINDOWS
    return error == WSAEWOULDBLOCK;
#else
    return error == EAGAIN || error == EWOULDBLOCK || error == EINTR;
#endif
}

int socket_pending_error(socket_t fd) {
    int code = 0;
#ifdef NET_PLATFORM_WINDOWS
    int size = sizeof(code);
#else
    socklen_t size = sizeof(code);
#endif
    if (::getsockopt(fd, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&code), &size) != 0)
        return GET_NET_SOCKET_ERROR();
    return code;
}

int poll_readable(std::vector<poll_descriptor>& descriptors, int timeout_ms) {
    if (timeout_ms < -1) return -1;
    // revents belongs to this invocation, even on timeout or native error.
    // Callers may reuse descriptors for successive readiness checks.
    for (auto& entry : descriptors) entry.revents = 0;
    if (descriptors.empty()) {
        if (timeout_ms > 0) std::this_thread::sleep_for(std::chrono::milliseconds(timeout_ms));
        // An empty set never waits indefinitely.
        return 0;
    }
#ifdef NET_PLATFORM_WINDOWS
    if (descriptors.size() > static_cast<size_t>(ULONG_MAX)) return -1;
    std::vector<WSAPOLLFD> native;
    native.reserve(descriptors.size());
    for (const auto& entry : descriptors) native.push_back({entry.fd, POLLRDNORM, 0});
    const int result = ::WSAPoll(native.data(), static_cast<ULONG>(native.size()), timeout_ms);
#else
    std::vector<pollfd> native;
    native.reserve(descriptors.size());
    for (const auto& entry : descriptors) native.push_back({entry.fd, POLLIN, 0});
    int result = -1;
    if (timeout_ms == -1) {
        do { result = ::poll(native.data(), static_cast<nfds_t>(native.size()), -1); }
        while (result < 0 && errno == EINTR);
    } else {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
        int remaining = timeout_ms;
        do {
            result = ::poll(native.data(), static_cast<nfds_t>(native.size()), remaining);
            if (result >= 0 || errno != EINTR) break;
            const auto now = std::chrono::steady_clock::now();
            if (now >= deadline) return 0;
            remaining = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count());
        } while (true);
    }
#endif
    if (result <= 0) return result;
    for (size_t i = 0; i < native.size(); ++i) descriptors[i].revents = native[i].revents;
    return result;
}
} // namespace aeon::internal
