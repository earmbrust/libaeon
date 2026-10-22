#pragma once

#include "socket.hpp"
#include "address.hpp"

namespace aeon::internal {

// TCP operations shared by ordinary and event-enabled socket classes.
// Implementation-only: never installed as a public API contract.
struct tcp_ops final {
    static bool connect(socket& owner, const char* hostname, int port);
    static bool connect(socket& owner);
    static bool connect(socket& owner, const address& addr);
    static int bind(socket& owner, const sockaddr* addr, socklen_t len);
    static bool listen(socket& owner, const char* ip, int port);
    static socket_t accept(socket& listener, int timeout_ms, bool blocking,
                           sockaddr_storage& peer);
};

} // namespace aeon::internal
