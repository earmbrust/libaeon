#include "udp_server_event_socket.hpp"
#include "internal/udp_ops.hpp"
#include "internal/udp_peer_store.hpp"
#include <algorithm>

namespace aeon {
udp_server_event_socket::udp_server_event_socket() : peers_(new internal::udp_peer_store) {}
udp_server_event_socket::~udp_server_event_socket() { close(); delete peers_; }
bool udp_server_event_socket::listen(int port) {
    const bool ok = internal::udp_ops::listen(*this, port);
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
    if (!is_valid_socket()) return nullptr;
    if (auto* existing = peers_->find(remote)) {
        if (existing->is_valid_socket()) return existing;
        // A new listener epoch may re-discover an endpoint from the old one.
        // The old borrowed pointer is invalidated by this replacement.
        remove_peer(existing);
    }
    const auto addr = remote.get_sockaddr_storage();
    if (addr.ss_family != AF_INET && addr.ss_family != AF_INET6) return nullptr;
    auto next = std::unique_ptr<udp_socket>(new udp_socket(this, remote));
    auto* borrowed = next.get();
    peers_->owned.push_back(std::move(next));
    try { if (!peers_->members.add(borrowed)) { peers_->owned.pop_back(); return nullptr; } }
    catch (...) { peers_->owned.pop_back(); throw; }
    return borrowed;
}
void udp_server_event_socket::register_sender(const address& remote) { (void)get_peer(remote); }
bool udp_server_event_socket::remove_peer(udp_socket* peer) {
    if (!peer) return false;
    auto& owned = peers_->owned;
    const auto it = std::find_if(owned.begin(), owned.end(),
        [peer](const auto& p) { return p.get() == peer; });
    if (it == owned.end()) return false;
    for (unsigned int i = 0; i < static_cast<unsigned int>(peers_->members.size()); ++i) {
        if (peers_->members.at(i) == peer) { peers_->members.remove(i); break; }
    }
    (*it)->close();
    owned.erase(it);
    return true;
}
socket_set& udp_server_event_socket::peers() { return peers_->members; }
const socket_set& udp_server_event_socket::peers() const { return peers_->members; }
} // namespace aeon
