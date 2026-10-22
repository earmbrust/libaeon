#include "udp_client_socket.hpp"
#include "internal/udp_ops.hpp"

namespace aeon {

udp_client_socket::udp_client_socket() = default;
udp_client_socket::udp_client_socket(const char* hostname, int port) {
    connect(hostname, port);
}
udp_client_socket::udp_client_socket(const std::string* hostname, int port) {
    if (hostname) connect(hostname->c_str(), port);
}
udp_client_socket::~udp_client_socket() = default;

bool udp_client_socket::connect(const char* hostname, int port) {
    return internal::udp_ops::connect(*this, hostname, port);
}
bool udp_client_socket::connect() {
    return internal::udp_ops::connect(*this);
}
bool udp_client_socket::connect(const address& target) {
    return internal::udp_ops::connect(*this, target);
}

} // namespace aeon
