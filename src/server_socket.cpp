#include "aeon.hpp"
#include "server_socket.hpp"
#include <memory>
#include <vector>
#include <algorithm>

namespace aeon {

server_socket::server_socket() = default;
server_socket::~server_socket() { close(); }

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
    if (ok) close_owned_peers();
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
    return add_peer(std::move(result));
}
bool server_socket::close() {
    const bool changed = socket::close();
    return close_owned_peers() || changed;
}
bool server_socket::remove_peer(socket* peer) { return remove_owned_peer(peer); }
socket_set& server_socket::peers() { return peer_members(); }
const socket_set& server_socket::peers() const { return peer_members(); }
int server_socket::peer_count() const { return owned_peer_count(); }
socket* server_socket::peer_at(unsigned int index) const { return owned_peer_at(index); }
} // namespace aeon
