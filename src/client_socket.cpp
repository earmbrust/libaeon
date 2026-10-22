#include "client_socket.hpp"
#include "internal/tcp_ops.hpp"

namespace aeon {

client_socket::client_socket()
    : socket(socket::default_family, socket::stream_type) {}

client_socket::client_socket(const char* hostname, int port)
    : client_socket() { connect(hostname, port); }

client_socket::client_socket(const std::string* hostname, int port)
    : client_socket() { if (hostname) connect(hostname->c_str(), port); }

client_socket::~client_socket() = default;

bool client_socket::connect(const char* hostname, int port) {
    return internal::tcp_ops::connect(*this, hostname, port);
}

bool client_socket::connect() {
    return internal::tcp_ops::connect(*this);
}

bool client_socket::connect(const address& addr) {
    return internal::tcp_ops::connect(*this, addr);
}

} // namespace aeon
