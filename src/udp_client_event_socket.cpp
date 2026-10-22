#include "udp_client_event_socket.hpp"
#include "internal/udp_ops.hpp"

namespace aeon {

udp_client_event_socket::udp_client_event_socket() = default;
udp_client_event_socket::udp_client_event_socket(const char* hostname, int port) {
    connect(hostname, port);
}
udp_client_event_socket::~udp_client_event_socket() = default;

bool udp_client_event_socket::connect(const char* hostname, int port) {
    const bool ok = internal::udp_ops::connect(*this, hostname, port);
    if (ok) fire_on_connect();
    return ok;
}
bool udp_client_event_socket::connect(const address& destination) {
    const bool ok = internal::udp_ops::connect(*this, destination);
    if (ok) fire_on_connect();
    return ok;
}
bool udp_client_event_socket::connect() {
    const bool ok = internal::udp_ops::connect(*this);
    if (ok) fire_on_connect();
    return ok;
}

} // namespace aeon
