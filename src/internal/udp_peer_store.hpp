#pragma once
#include "udp_socket.hpp"
#include "socket_set.hpp"
#include <memory>
#include <vector>
#include <cstring>
#include <algorithm>

namespace aeon::internal {
inline bool same_udp_address(const address& a, const address& b) {
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
struct udp_peer_store {
    std::vector<std::unique_ptr<udp_socket>> owned;
    socket_set members; // Non-owning view; owned is destroyed after members.
    udp_socket* get_or_create(udp_socket* server, const address& remote) {
        if (!server->is_valid_socket()) return nullptr;
        if (auto* existing = find(remote)) {
            if (existing->is_valid_socket()) return existing;
            remove(existing);
        }
        const auto addr = remote.get_sockaddr_storage();
        if (addr.ss_family != AF_INET && addr.ss_family != AF_INET6) return nullptr;
        auto next = std::unique_ptr<udp_socket>(new udp_socket(server, remote));
        auto* borrowed = next.get();
        owned.push_back(std::move(next));
        try {
            if (!members.add(borrowed)) {
                owned.pop_back();
                return nullptr;
            }
        } catch (...) {
            owned.pop_back();
            throw;
        }
        return borrowed;
    }
    bool remove(udp_socket* peer) {
        if (!peer) return false;
        const auto it = std::find_if(owned.begin(), owned.end(),
            [peer](const auto& entry) { return entry.get() == peer; });
        if (it == owned.end()) return false;
        for (unsigned int i = 0; i < static_cast<unsigned int>(members.size()); ++i) {
            if (members.at(i) == peer) {
                members.remove(i);
                break;
            }
        }
        (*it)->close();
        owned.erase(it);
        return true;
    }
    udp_socket* find(const address& target) const {
        for (auto& p : owned)
            if (same_udp_address(p->get_remote_address(), target)) return p.get();
        return nullptr;
    }
};
} // namespace aeon::internal
