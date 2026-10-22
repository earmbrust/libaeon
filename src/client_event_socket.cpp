#include "client_event_socket.hpp"
#include "internal/tcp_ops.hpp"

namespace aeon {

client_event_socket::client_event_socket() = default;
client_event_socket::client_event_socket(const char* hostname, int port)
    : client_event_socket() { connect(hostname, port); }
client_event_socket::~client_event_socket() { stop_polling(); }

bool client_event_socket::connect() {
    stop_polling();
    if (!internal::tcp_ops::connect(*this)) return false;
    if (set_blocking(false) != 0) { socket::close(); return false; }
    start_polling();
    fire_on_connect();
    return true;
}

bool client_event_socket::connect(const char* hostname, int port) {
    stop_polling();
    if (!internal::tcp_ops::connect(*this, hostname, port)) return false;
    if (set_blocking(false) != 0) { socket::close(); return false; }
    start_polling();
    fire_on_connect();
    return true;
}

bool client_event_socket::connect(const address& addr) {
    stop_polling();
    if (!internal::tcp_ops::connect(*this, addr)) return false;
    if (set_blocking(false) != 0) { socket::close(); return false; }
    start_polling();
    fire_on_connect();
    return true;
}

bool client_event_socket::close() {
    stop_polling();
    const bool result = socket::close();
    if (result) fire_on_close();
    return result;
}

void client_event_socket::on_data_impl(const char*, int) {}
void client_event_socket::on_connect_impl() {}
void client_event_socket::on_close_impl() {}
void client_event_socket::on_error_impl(int) {}

} // namespace aeon
