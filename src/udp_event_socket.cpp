#include "udp_event_socket.hpp"

namespace aeon {

udp_event_socket::udp_event_socket() {
    if (is_valid_socket() && set_blocking(false) != 0) {
        close();
    }
}
udp_event_socket::~udp_event_socket() = default;

} // namespace aeon
