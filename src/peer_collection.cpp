#include "aeon.hpp"
#include "internal/peer_collection.hpp"
#include <algorithm>
#include <cstring>
#include <memory>
#include <vector>

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

struct peer_collection::state {
    std::vector<std::unique_ptr<socket>> owned;
    socket_set members;
};

peer_collection::peer_collection() : peers_(new state) {}
peer_collection::~peer_collection() { delete peers_; }

socket* peer_collection::add_peer(std::unique_ptr<socket> peer) {
    if (!peer) return nullptr;
    socket* borrowed = peer.get();
    peers_->owned.push_back(std::move(peer));
    try {
        if (!peers_->members.add(borrowed)) {
            peers_->owned.pop_back();
            return nullptr;
        }
    } catch (...) {
        peers_->owned.pop_back();
        throw;
    }
    return borrowed;
}

bool peer_collection::remove_owned_peer(socket* peer) {
    if (!peer) return false;
    auto& owned = peers_->owned;
    const auto it = std::find_if(owned.begin(), owned.end(),
        [peer](const auto& entry) { return entry.get() == peer; });
    if (it == owned.end()) return false;
    for (unsigned int i = 0; i < static_cast<unsigned int>(peers_->members.size()); ++i) {
        if (peers_->members.at(i) == peer) {
            peers_->members.remove(i);
            break;
        }
    }
    (*it)->close();
    owned.erase(it);
    return true;
}

bool peer_collection::close_owned_peers() {
    bool changed = false;
    for (auto& peer : peers_->owned) changed = peer->close() || changed;
    return changed;
}

socket_set& peer_collection::peer_members() { return peers_->members; }
const socket_set& peer_collection::peer_members() const { return peers_->members; }
int peer_collection::owned_peer_count() const {
    return static_cast<int>(peers_->owned.size());
}
socket* peer_collection::owned_peer_at(unsigned int index) const {
    return index < peers_->owned.size() ? peers_->owned[index].get() : nullptr;
}
socket* peer_collection::find_udp_peer(const address& remote) const {
    for (const auto& peer : peers_->owned) {
        if (same_udp_address(peer->get_remote_address(), remote)) return peer.get();
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
