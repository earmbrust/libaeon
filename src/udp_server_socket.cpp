/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/
#include "udp_server_socket.hpp"
#include "udp_server_event_socket.hpp"
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

namespace aeon {
udp_server_socket::udp_server_socket() : peers_(new internal::udp_peer_store) {}
udp_server_socket::~udp_server_socket() { close(); delete peers_; }
bool udp_server_socket::listen(int port) {
    // Existing logical peers cannot survive a rebind as active endpoints.
    const bool ok = udp_listen(port);
    if (ok) for (auto& p : peers_->owned) p->close();
    return ok;
}
bool udp_server_socket::listen() { return listen(port); }
bool udp_server_socket::close() {
    bool changed = udp_socket::close();
    for (auto& p : peers_->owned) changed = p->close() || changed;
    return changed;
}
udp_socket* udp_server_socket::get_peer(const address& remote) {
    return peers_->get_or_create(this, remote);
}
bool udp_server_socket::remove_peer(udp_socket* peer) {
    return peers_->remove(peer);
}
socket_set& udp_server_socket::peers() { return peers_->members; }
const socket_set& udp_server_socket::peers() const { return peers_->members; }
int udp_server_socket::read(char* data, int size) {
    const int n = udp_socket::read(data, size);
    if (n >= 0) (void)get_peer(get_remote_address());
    return n;
}
int udp_server_socket::read() {
    const int n = read(inbuffer, max_buffer_size - 1);
    if (n >= 0) inbuffer[n] = '\0';
    return n;
}
std::string udp_server_socket::read(int size) {
    if (size <= 0) return {};
    std::string buffer(static_cast<std::size_t>(size), '\0');
    const int n = read(buffer.data(), size);
    if (n <= 0) return {};
    buffer.resize(static_cast<std::size_t>(n));
    return buffer;
}

udp_server_event_socket::udp_server_event_socket() : peers_(new internal::udp_peer_store) {}
udp_server_event_socket::~udp_server_event_socket() { close(); delete peers_; }
bool udp_server_event_socket::listen(int port) {
    const bool ok = udp_listen(port);
    if (ok) {
        for (auto& p : peers_->owned) p->close();
        fire_on_listen();
    }
    return ok;
}
bool udp_server_event_socket::listen() { return listen(port); }
bool udp_server_event_socket::close() {
    bool changed = udp_event_socket::close();
    for (auto& p : peers_->owned) changed = p->close() || changed;
    return changed;
}
int udp_server_event_socket::read(char* data, int size) {
    const int count = udp_socket::read(data, size);
    if (count >= 0) (void)get_peer(get_remote_address());
    return count;
}
int udp_server_event_socket::read() {
    const int count = read(inbuffer, max_buffer_size - 1);
    if (count >= 0) inbuffer[count] = '\0';
    return count;
}
std::string udp_server_event_socket::read(int size) {
    if (size <= 0) return {};
    std::string buffer(static_cast<std::size_t>(size), '\0');
    const int count = read(buffer.data(), size);
    if (count <= 0) return {};
    buffer.resize(static_cast<std::size_t>(count));
    return buffer;
}
udp_socket* udp_server_event_socket::get_peer(const address& remote) {
    return peers_->get_or_create(this, remote);
}
void udp_server_event_socket::register_sender(const address& remote) { (void)get_peer(remote); }
bool udp_server_event_socket::remove_peer(udp_socket* peer) {
    return peers_->remove(peer);
}
socket_set& udp_server_event_socket::peers() { return peers_->members; }
const socket_set& udp_server_event_socket::peers() const { return peers_->members; }
} // namespace aeon
