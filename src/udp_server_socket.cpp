#include "udp_server_socket.hpp"
#include "internal/udp_ops.hpp"

namespace aeon {

udp_server_socket::udp_server_socket() = default;
udp_server_socket::~udp_server_socket() = default;

bool udp_server_socket::listen(int port) {
    return internal::udp_ops::listen(*this, port);
}
bool udp_server_socket::listen() {
    return listen(port);
}

} // namespace aeon
