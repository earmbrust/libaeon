#include "udp_client_event_socket.hpp"
#include "internal/udp_ops.hpp"

namespace aeon {

udp_client_event_socket::udp_client_event_socket() = default;
udp_client_event_socket::udp_client_event_socket(const char* hostname, int port) {
    connect(hostname, port);
}
udp_client_event_socket::~udp_client_event_socket() = default;

bool udp_client_event_socket::connect(const char* hostname, int port) {
    return internal::udp_ops::connect(*this, hostname, port);
}
bool udp_client_event_socket::connect(const address& destination) {
    return internal::udp_ops::connect(*this, destination);
}
bool udp_client_event_socket::connect() {
    return internal::udp_ops::connect(*this);
}

} // namespace aeon
