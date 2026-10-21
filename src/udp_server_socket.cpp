/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/
#include "aeon.hpp"
#include "udp_server_socket.hpp"
#include <memory>
#include <cstring>

namespace aeon::internal {

static bool same_udp_address(const address& a, const address& b) {
    const auto lhs = a.get_sockaddr_storage();
    const auto rhs = b.get_sockaddr_storage();
    if (a.get_port() != b.get_port()) return false;
    if (lhs.ss_family == AF_INET && rhs.ss_family == AF_INET) {
        return reinterpret_cast<const sockaddr_in*>(&lhs)->sin_addr.s_addr ==
               reinterpret_cast<const sockaddr_in*>(&rhs)->sin_addr.s_addr;
    }
    if (lhs.ss_family == AF_INET6 && rhs.ss_family == AF_INET6) {
        const auto* l = reinterpret_cast<const sockaddr_in6*>(&lhs);
        const auto* r = reinterpret_cast<const sockaddr_in6*>(&rhs);
        return l->sin6_scope_id == r->sin6_scope_id &&
               std::memcmp(&l->sin6_addr, &r->sin6_addr, sizeof(l->sin6_addr)) == 0;
    }
    // Treat IPv4 and an equivalent IPv4-mapped IPv6 endpoint as one peer.
    // Dual-stack UDP listeners receive IPv4 senders as ::ffff:a.b.c.d.
    const auto& v6 = lhs.ss_family == AF_INET6 ? lhs : rhs;
    const auto& v4 = lhs.ss_family == AF_INET ? lhs : rhs;
    if (v6.ss_family != AF_INET6 || v4.ss_family != AF_INET) return false;
    const auto* ipv6 = reinterpret_cast<const sockaddr_in6*>(&v6);
    const auto* ipv4 = reinterpret_cast<const sockaddr_in*>(&v4);
    const unsigned char* p = ipv6->sin6_addr.s6_addr;
    static constexpr unsigned char mapped_prefix[12] =
        {0,0,0,0,0,0,0,0,0,0,0xff,0xff};
    return ipv6->sin6_scope_id == 0 &&
           std::memcmp(p, mapped_prefix, sizeof(mapped_prefix)) == 0 &&
           std::memcmp(p + 12, &ipv4->sin_addr, sizeof(ipv4->sin_addr)) == 0;
}

socket* peer_collection::find_udp_peer(const address& remote) const {
    const auto& members = peer_members();
    for (unsigned int i = 0; i < static_cast<unsigned int>(members.size()); ++i) {
        socket* peer = members.at(i);
        if (peer && same_udp_address(peer->get_remote_address(), remote)) return peer;
    }
    return nullptr;
}

udp_socket* peer_collection::get_or_create_udp_peer(udp_socket* server, const address& remote) {
    if (!server || !server->is_valid_socket()) return nullptr;
    if (auto* existing = static_cast<udp_socket*>(find_udp_peer(remote))) {
        if (existing->is_valid_socket()) return existing;
        remove_owned_peer(existing);
    }
    const auto endpoint = remote.get_sockaddr_storage();
    if (endpoint.ss_family != AF_INET && endpoint.ss_family != AF_INET6) return nullptr;
    return static_cast<udp_socket*>(add_peer(std::unique_ptr<udp_socket>(new udp_socket(server, remote))));
}

} // namespace aeon::internal

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
