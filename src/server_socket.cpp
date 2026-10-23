#include "server_socket.hpp"
#include <memory>
#include <vector>
#include <algorithm>

namespace aeon {
namespace internal { struct tcp_peer_store { std::vector<std::unique_ptr<socket>> peers; socket_set members; }; }

server_socket::server_socket() : peers_(new internal::tcp_peer_store) {}
server_socket::~server_socket() { close(); delete peers_; }

int server_socket::set_accept_timeout(int timeout_ms) {
    if (timeout_ms < 0 || timeout_ms > 60000) { set_error(err_no_socket); return -1; }
    accept_timeout_ms = timeout_ms;
    set_error(err_none);
    return 0;
}
int server_socket::bind(const sockaddr* addr, socklen_t length) {
    return tcp_bind( addr, length);
}
bool server_socket::listen() { return listen(port); }
bool server_socket::listen(int port) { return listen("0.0.0.0", port); }
bool server_socket::listen(const char* ip, int port) {
    // Do not leave live peers attached to a replaced listener.
    const bool ok = tcp_listen( ip, port);
    if (ok) for (auto& p : peers_->peers) p->close();
    return ok;
}
socket* server_socket::accept() { return accept(false); }
socket* server_socket::accept(bool blocking) {
    sockaddr_storage peer{};
    const socket_t fd = tcp_accept( accept_timeout_ms, blocking, peer);
    if (fd == invalid_socket) return nullptr;
    // Unique owner is established immediately; no duplicate close on failures.
    std::unique_ptr<socket> result;
    try { result = std::make_unique<socket>(fd, true); }
    catch (...) { NET_CLOSE_SOCKET(fd); throw; }
    result->remote_addr = peer;
    result->set_parent_server(this);
    if (result->set_blocking(blocking) != 0) return nullptr;
    auto* borrowed = result.get();
    peers_->peers.push_back(std::move(result));
    try {
        if (!peers_->members.add(borrowed)) { peers_->peers.pop_back(); return nullptr; }
    } catch (...) { peers_->peers.pop_back(); throw; }
    return borrowed;
}
bool server_socket::close() {
    bool changed = socket::close();
    for (auto& peer : peers_->peers) changed = peer->close() || changed;
    return changed;
}
bool server_socket::remove_peer(socket* peer) {
    if (!peer) return false;
    auto& owned = peers_->peers;
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
socket_set& server_socket::peers() { return peers_->members; }
const socket_set& server_socket::peers() const { return peers_->members; }
int server_socket::peer_count() const { return static_cast<int>(peers_->peers.size()); }
socket* server_socket::peer_at(unsigned int index) const {
    return index < peers_->peers.size() ? peers_->peers[index].get() : nullptr;
}
} // namespace aeon
