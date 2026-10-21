#include "aeon.hpp"
#include "server_event_socket.hpp"
#include <memory>
#include <vector>
#include <algorithm>

namespace aeon {

server_event_socket::server_event_socket() = default;
server_event_socket::server_event_socket(int port) : server_event_socket() { listen(port); }
server_event_socket::~server_event_socket() { close(); }

int server_event_socket::set_accept_timeout(int ms) {
    if (ms < 0 || ms > 60000) { set_error(err_no_socket); return -1; }
    accept_timeout_ms_ = ms;
    set_error(err_none);
    return 0;
}
int server_event_socket::bind(const sockaddr* addr, socklen_t len) {
    return tcp_bind( addr, len);
}
bool server_event_socket::listen() { return listen(port); }
bool server_event_socket::listen(int port) { return listen("0.0.0.0", port); }
bool server_event_socket::listen(const char* ip, int port) {
    if (!tcp_listen( ip, port)) return false;
    close_owned_peers();
    if (set_blocking(false) != 0) { socket::close(); return false; }
    fire_on_listen();
    return true;
}
event_socket* server_event_socket::accept() { return accept(false); }
event_socket* server_event_socket::accept(bool blocking) {
    sockaddr_storage peer{};
    const socket_t fd = tcp_accept( accept_timeout_ms_, blocking, peer);
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
    return static_cast<event_socket*>(add_peer(std::move(result)));
}
bool server_event_socket::close() {
    const bool changed = event_socket::close();
    return close_owned_peers() || changed;
}
bool server_event_socket::remove_peer(event_socket* peer) { return remove_owned_peer(peer); }
socket_set& server_event_socket::peers() { return peer_members(); }
const socket_set& server_event_socket::peers() const { return peer_members(); }
int server_event_socket::peer_count() const { return owned_peer_count(); }
event_socket* server_event_socket::peer_at(unsigned int index) const {
    return static_cast<event_socket*>(owned_peer_at(index));
}
} // namespace aeon
