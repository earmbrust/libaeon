/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/
#include "aeon.hpp"
#include "udp_server_socket.hpp"
#include <memory>

namespace aeon {
udp_server_socket::udp_server_socket() = default;
udp_server_socket::~udp_server_socket() { close(); }
bool udp_server_socket::listen(int port) {
    // Existing logical peers cannot survive a rebind as active endpoints.
    const bool ok = udp_listen(port);
    if (ok) close_owned_peers();
    return ok;
}
bool udp_server_socket::listen() { return listen(port); }
bool udp_server_socket::close() {
    bool changed = udp_socket::close();
    changed = close_owned_peers() || changed;
    return changed;
}
udp_socket* udp_server_socket::get_peer(const address& remote) {
    return get_or_create_udp_peer(this, remote);
}

bool udp_server_socket::remove_peer(udp_socket* peer) {
    return remove_owned_peer(peer);
}
socket_set& udp_server_socket::peers() { return peer_members(); }
const socket_set& udp_server_socket::peers() const { return peer_members(); }
int udp_server_socket::read(char* data, int size) {
    const int n = udp_socket::read(data, size);
    if (n >= 0) (void)get_peer(get_remote_address());
    return n;
}
} // namespace aeon
