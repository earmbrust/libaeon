#pragma once

#include "udp_socket.hpp"

namespace aeon {

// Datagram transport for application-controlled event dispatch. Callback
// registration and multi-socket readiness dispatch are introduced separately.
class LIBAEON_API udp_event_socket : public udp_socket {
public:
    udp_event_socket();
    ~udp_event_socket() override;
};

} // namespace aeon
