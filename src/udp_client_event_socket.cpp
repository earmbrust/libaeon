#include "udp_client_event_socket.hpp"

namespace aeon {

udp_client_event_socket::udp_client_event_socket() = default;
udp_client_event_socket::udp_client_event_socket(const char* hostname, int port) {
    connect(hostname, port);
}
udp_client_event_socket::~udp_client_event_socket() = default;

bool udp_client_event_socket::connect(const char* hostname, int port) {
    const bool ok = udp_connect( hostname, port);
    if (ok) fire_on_connect();
    return ok;
}
bool udp_client_event_socket::connect(const address& destination) {
    const bool ok = udp_connect( destination);
    if (ok) fire_on_connect();
    return ok;
}
bool udp_client_event_socket::connect() {
    const bool ok = udp_connect();
    if (ok) fire_on_connect();
    return ok;
}

} // namespace aeon
