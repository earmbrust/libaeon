#pragma once

#include "udp_event_socket.hpp"

namespace aeon {

class LIBAEON_API udp_server_event_socket : public udp_event_socket {
public:
    udp_server_event_socket();
    ~udp_server_event_socket() override;

    bool listen(int port);
    bool listen();
};

} // namespace aeon
