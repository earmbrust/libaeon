#include "udp_server_socket.hpp"
#include "internal/udp_peer_store.hpp"

namespace aeon {
udp_server_socket::udp_server_socket() : peers_(new internal::udp_peer_store) {}
udp_server_socket::~udp_server_socket() { close(); delete peers_; }
bool udp_server_socket::listen(int port) {
    // Existing logical peers cannot survive a rebind as active endpoints.
    const bool ok = udp_listen( port);
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
} // namespace aeon
