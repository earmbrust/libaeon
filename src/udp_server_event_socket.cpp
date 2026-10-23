#include "udp_server_event_socket.hpp"
#include "internal/udp_peer_store.hpp"

namespace aeon {
udp_server_event_socket::udp_server_event_socket() : peers_(new internal::udp_peer_store) {}
udp_server_event_socket::~udp_server_event_socket() { close(); delete peers_; }
bool udp_server_event_socket::listen(int port) {
    const bool ok = udp_listen( port);
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
