/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#include <aeon.hpp>

// Stringification helpers for the public aeon::get_library_version() API.
#define STR_(x) #x
#define STR(x) STR_(x)

namespace aeon {
    const char* get_library_version() {
        return STR(VERSION);
    };
}

// Keep native socket errors inside libAeon. Public errors use stable codes.
namespace aeon::internal {
int translate_native_error(int native_error) noexcept {
    if (native_error == 0) return err_none;
#ifdef NET_PLATFORM_WINDOWS
    switch (native_error) {
        case WSAEBADF:
        case WSAENOTSOCK: return err_no_socket;
        case WSAEINVAL: return err_invalid_argument;
        case WSAETIMEDOUT: return err_timeout;
        case WSAEWOULDBLOCK: return err_would_block;
        case WSAEINTR: return err_interrupted;
        case WSAECONNREFUSED: return err_connection_refused;
        case WSAECONNRESET: return err_connection_reset;
        case WSAECONNABORTED: return err_connection_aborted;
        case WSAENOTCONN: return err_not_connected;
        case WSAEADDRINUSE: return err_address_in_use;
        case WSAENETUNREACH: return err_network_unreachable;
        case WSAEHOSTUNREACH: return err_host_unreachable;
        case WSAEACCES: return err_permission_denied;
        case WSAEMSGSIZE: return err_message_too_large;
        case WSAEADDRNOTAVAIL: return err_address_not_available;
        case WSAEISCONN: return err_already_connected;
        case WSAEINPROGRESS:
        case WSAEALREADY: return err_in_progress;
        case WSAESHUTDOWN: return err_broken_pipe;
        case WSAENETDOWN: return err_network_down;
        case WSAENETRESET: return err_network_reset;
        case WSAENOBUFS: return err_no_buffers;
        case WSAEMFILE: return err_resource_limit;
        case WSAEOPNOTSUPP:
        case WSAEPROTONOSUPPORT:
        case WSAEAFNOSUPPORT: return err_unsupported;
        case WSAEPROTOTYPE: return err_protocol;
        default: return err_system;
    }
#else
    // if/else avoids duplicate case labels when POSIX aliases share a value.
    if (native_error == EBADF || native_error == ENOTSOCK) return err_no_socket;
    if (native_error == EINVAL) return err_invalid_argument;
    if (native_error == ETIMEDOUT) return err_timeout;
    if (native_error == EAGAIN || native_error == EWOULDBLOCK) return err_would_block;
    if (native_error == EINTR) return err_interrupted;
    if (native_error == ECONNREFUSED) return err_connection_refused;
    if (native_error == ECONNRESET) return err_connection_reset;
    if (native_error == ECONNABORTED) return err_connection_aborted;
    if (native_error == ENOTCONN) return err_not_connected;
    if (native_error == EADDRINUSE) return err_address_in_use;
    if (native_error == ENETUNREACH) return err_network_unreachable;
    if (native_error == EHOSTUNREACH) return err_host_unreachable;
    if (native_error == EACCES || native_error == EPERM) return err_permission_denied;
    if (native_error == EMSGSIZE) return err_message_too_large;
    if (native_error == EADDRNOTAVAIL) return err_address_not_available;
    if (native_error == EISCONN) return err_already_connected;
    if (native_error == EINPROGRESS || native_error == EALREADY) return err_in_progress;
    if (native_error == EPIPE) return err_broken_pipe;
    if (native_error == ENETDOWN) return err_network_down;
    if (native_error == ENETRESET) return err_network_reset;
    if (native_error == ENOBUFS) return err_no_buffers;
    if (native_error == EMFILE || native_error == ENFILE) return err_resource_limit;
    if (native_error == ENOTSUP || native_error == EOPNOTSUPP ||
        native_error == EPROTONOSUPPORT || native_error == EAFNOSUPPORT)
        return err_unsupported;
#ifdef EPROTO
    if (native_error == EPROTO) return err_protocol;
#endif
    return err_system;
#endif
}
} // namespace aeon::internal
