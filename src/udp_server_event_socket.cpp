/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/
#include "aeon.hpp"
#include "udp_server_event_socket.hpp"
#include <memory>

namespace aeon {
udp_server_event_socket::udp_server_event_socket() = default;
udp_server_event_socket::~udp_server_event_socket() { close(); }
bool udp_server_event_socket::listen(int port) {
    const bool ok = udp_listen(port);
    if (ok) {
        close_owned_peers();
        fire_on_listen();
    }
    return ok;
}
bool udp_server_event_socket::listen() { return listen(port); }
bool udp_server_event_socket::close() {
    bool changed = udp_event_socket::close();
    changed = close_owned_peers() || changed;
    return changed;
}
int udp_server_event_socket::read(char* data, int size) {
    const int count = udp_socket::read(data, size);
    if (count >= 0) (void)get_peer(get_remote_address());
    return count;
}
udp_socket* udp_server_event_socket::get_peer(const address& remote) {
    return get_or_create_udp_peer(this, remote);
}

void udp_server_event_socket::register_sender(const address& remote) { (void)get_peer(remote); }
bool udp_server_event_socket::remove_peer(udp_socket* peer) {
    return remove_owned_peer(peer);
}
socket_set& udp_server_event_socket::peers() { return peer_members(); }
const socket_set& udp_server_event_socket::peers() const { return peer_members(); }
} // namespace aeon
