#include "server_socket.hpp"
#include "internal/tcp_ops.hpp"

namespace aeon {

server_socket::server_socket() = default;
server_socket::~server_socket() = default;

int server_socket::set_accept_timeout(int timeout_ms) {
    if (timeout_ms < 0 || timeout_ms > 60000) {
        set_error(err_no_socket);
        return -1;
    }
    accept_timeout_ms = timeout_ms;
    set_error(err_none);
    return 0;
}

int server_socket::bind(const sockaddr* addr, socklen_t length) {
    return internal::tcp_ops::bind(*this, addr, length);
}

bool server_socket::listen() { return listen(port); }
bool server_socket::listen(int port) { return listen("0.0.0.0", port); }
bool server_socket::listen(const char* ip, int port) {
    return internal::tcp_ops::listen(*this, ip, port);
}

std::unique_ptr<socket> server_socket::accept() { return accept(false); }

std::unique_ptr<socket> server_socket::accept(bool blocking) {
    sockaddr_storage peer{};
    const socket_t fd = internal::tcp_ops::accept(*this, accept_timeout_ms,
                                                 blocking, peer);
    if (fd == invalid_socket) return {};
    try {
        auto result = std::make_unique<socket>(fd, true);
        result->remote_addr = peer;
        if (result->set_blocking(blocking) != 0) return {};
        return result;
    } catch (...) {
        NET_CLOSE_SOCKET(fd);
        throw;
    }
}

} // namespace aeon
