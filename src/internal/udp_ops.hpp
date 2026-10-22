#pragma once

#include "socket.hpp"

namespace aeon::internal {

// Shared native UDP ownership and destination management.
// The socket base owns at most one descriptor; replacements are committed only
// after a candidate has been configured successfully.
struct udp_ops final {
    static bool connect(socket& owner, const char* hostname, int port);
    static bool connect(socket& owner, const address& remote);
    static bool connect(socket& owner);
    static bool listen(socket& owner, int port);

    // Internal test seam for exercising native bind failures deterministically.
    // Not part of Aeon's installed public API.
    using native_bind_fn = int (*)(socket_t, const sockaddr*, socklen_t);
    static bool listen(socket& owner, int port, native_bind_fn bind_socket);
};

} // namespace aeon::internal
