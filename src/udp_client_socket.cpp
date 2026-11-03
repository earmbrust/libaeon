#include "aeon.hpp"
#include "udp_client_socket.hpp"

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
    return udp_connect( hostname, port);
}
bool udp_client_socket::connect() {
    return udp_connect();
}
bool udp_client_socket::connect(const address& target) {
    return udp_connect( target);
}

} // namespace aeon
