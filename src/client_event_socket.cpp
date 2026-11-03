#include "aeon.hpp"
#include "client_event_socket.hpp"

namespace aeon {

client_event_socket::client_event_socket() = default;
client_event_socket::client_event_socket(const char* hostname, int port)
    : client_event_socket() { connect(hostname, port); }
client_event_socket::~client_event_socket() = default;

bool client_event_socket::connect() {
    if (!tcp_connect()) return false;
    return finish_connect();
}

bool client_event_socket::connect(const char* hostname, int port) {
    if (!tcp_connect( hostname, port)) return false;
    return finish_connect();
}

bool client_event_socket::connect(const address& addr) {
    if (!tcp_connect( addr)) return false;
    return finish_connect();
}

bool client_event_socket::finish_connect() {
    if (set_blocking(false) != 0) { socket::close(); return false; }
    fire_on_connect();
    return true;
}

bool client_event_socket::close() {
    return event_socket::close();
}


} // namespace aeon
