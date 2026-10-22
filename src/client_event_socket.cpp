#include "client_event_socket.hpp"
#include "internal/tcp_ops.hpp"

namespace aeon {

client_event_socket::client_event_socket() = default;
client_event_socket::client_event_socket(const char* hostname, int port)
    : client_event_socket() { connect(hostname, port); }
client_event_socket::~client_event_socket() = default;

bool client_event_socket::connect() {
    if (!internal::tcp_ops::connect(*this)) return false;
    if (set_blocking(false) != 0) { socket::close(); return false; }
    fire_on_connect();
    return true;
}

bool client_event_socket::connect(const char* hostname, int port) {
    if (!internal::tcp_ops::connect(*this, hostname, port)) return false;
    if (set_blocking(false) != 0) { socket::close(); return false; }
    fire_on_connect();
    return true;
}

bool client_event_socket::connect(const address& addr) {
    if (!internal::tcp_ops::connect(*this, addr)) return false;
    if (set_blocking(false) != 0) { socket::close(); return false; }
    fire_on_connect();
    return true;
}

bool client_event_socket::close() {
    return event_socket::close();
}

void client_event_socket::on_data_impl(const char*, int) {}
void client_event_socket::on_connect_impl() {}
void client_event_socket::on_close_impl() {}
void client_event_socket::on_error_impl(int) {}

} // namespace aeon
