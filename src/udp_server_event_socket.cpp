#include "udp_server_event_socket.hpp"
#include "internal/udp_ops.hpp"

namespace aeon {

udp_server_event_socket::udp_server_event_socket() = default;
udp_server_event_socket::~udp_server_event_socket() = default;

bool udp_server_event_socket::listen(int port) {
    return internal::udp_ops::listen(*this, port);
}
bool udp_server_event_socket::listen() {
    return listen(port);
}

} // namespace aeon
