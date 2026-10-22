#include "server_event_socket.hpp"
#include "internal/tcp_ops.hpp"

namespace aeon {

server_event_socket::server_event_socket() = default;
server_event_socket::server_event_socket(int port)
    : server_event_socket() { listen(port); }
server_event_socket::~server_event_socket() { stop_polling(); }

int server_event_socket::set_accept_timeout(int ms) {
    if (ms < 0 || ms > 60000) {
        set_error(err_no_socket);
        return -1;
    }
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
    stop_polling(); // Listening sockets are not connected data streams.
    if (!internal::tcp_ops::listen(*this, ip, port)) return false;
    if (set_blocking(false) != 0) { socket::close(); return false; }
    fire_on_listen();
    return true;
}

std::unique_ptr<event_socket> server_event_socket::accept() {
    return accept(false);
}

std::unique_ptr<event_socket> server_event_socket::accept(bool blocking) {
    sockaddr_storage peer{};
    const socket_t fd = internal::tcp_ops::accept(*this, accept_timeout_ms_,
                                                 blocking, peer);
    if (fd == invalid_socket) return {};
    // Adopt directly; do not allocate another native descriptor.
    class adopted_event_socket final : public event_socket {
    public:
        adopted_event_socket(socket_t native, bool adopt)
            : event_socket(native, adopt) {}
    };
    try {
        auto result = std::make_unique<adopted_event_socket>(fd, true);
        result->remote_addr = peer;
        if (result->set_blocking(blocking) != 0) return {};
        return result;
    } catch (...) {
        NET_CLOSE_SOCKET(fd);
        throw;
    }
}

bool server_event_socket::close() {
    stop_polling();
    const bool result = socket::close();
    if (result) fire_on_close();
    return result;
}

void server_event_socket::on_data_impl(const char*, int) {}
void server_event_socket::on_listen_impl() {}
void server_event_socket::on_close_impl() {}
void server_event_socket::on_error_impl(int) {}

} // namespace aeon
