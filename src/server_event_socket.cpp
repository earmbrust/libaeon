#include "server_event_socket.hpp"
#include "internal/tcp_ops.hpp"
#include <memory>
#include <vector>
#include <algorithm>

namespace aeon {
namespace internal { struct event_peer_store { std::vector<std::unique_ptr<event_socket>> peers; socket_set members; }; }

server_event_socket::server_event_socket() : peers_(new internal::event_peer_store) {}
server_event_socket::server_event_socket(int port) : server_event_socket() { listen(port); }
server_event_socket::~server_event_socket() { close(); delete peers_; }

int server_event_socket::set_accept_timeout(int ms) {
    if (ms < 0 || ms > 60000) { set_error(err_no_socket); return -1; }
    accept_timeout_ms_ = ms;
    set_error(err_none);
    return 0;
}
int server_event_socket::bind(const sockaddr* addr, socklen_t len) {
    return internal::tcp_ops::bind(*this, addr, len);
}
bool server_event_socket::listen() { return listen(port); }
bool server_event_socket::listen(int port) { return listen("0.0.0.0", port); }
bool server_event_socket::listen(const char* ip, int port) {
    if (!internal::tcp_ops::listen(*this, ip, port)) return false;
    for (auto& p : peers_->peers) p->close();
    if (set_blocking(false) != 0) { socket::close(); return false; }
    fire_on_listen();
    return true;
}
event_socket* server_event_socket::accept() { return accept(false); }
event_socket* server_event_socket::accept(bool blocking) {
    sockaddr_storage peer{};
    const socket_t fd = internal::tcp_ops::accept(*this, accept_timeout_ms_, blocking, peer);
    if (fd == invalid_socket) return nullptr;
    class adopted_event_socket final : public event_socket {
    public:
        adopted_event_socket(socket_t native, bool adopt) : event_socket(native, adopt) {}
    };
    std::unique_ptr<event_socket> result;
    try { result = std::make_unique<adopted_event_socket>(fd, true); }
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
bool server_event_socket::close() {
    bool changed = event_socket::close();
    for (auto& p : peers_->peers) changed = p->close() || changed;
    return changed;
}
bool server_event_socket::remove_peer(event_socket* peer) {
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
socket_set& server_event_socket::peers() { return peers_->members; }
const socket_set& server_event_socket::peers() const { return peers_->members; }
int server_event_socket::peer_count() const { return static_cast<int>(peers_->peers.size()); }
event_socket* server_event_socket::peer_at(unsigned int index) const {
    return index < peers_->peers.size() ? peers_->peers[index].get() : nullptr;
}
void server_event_socket::on_data_impl(const char*, int) {}
void server_event_socket::on_listen_impl() {}
void server_event_socket::on_close_impl() {}
void server_event_socket::on_error_impl(int) {}
} // namespace aeon
