#include "aeon.hpp"
#include "client_socket.hpp"

namespace aeon {

client_socket::client_socket()
    : socket(socket::default_family, socket::stream_type) {}

client_socket::client_socket(const char* hostname, int port)
    : client_socket() { connect(hostname, port); }

client_socket::client_socket(const std::string* hostname, int port)
    : client_socket() { if (hostname) connect(hostname->c_str(), port); }

client_socket::~client_socket() = default;

bool client_socket::connect(const char* hostname, int port) {
    return tcp_connect( hostname, port);
}

bool client_socket::connect() {
    return tcp_connect();
}

bool client_socket::connect(const address& addr) {
    return tcp_connect( addr);
}

} // namespace aeon
