#pragma once

#include "udp_event_socket.hpp"

namespace aeon {

class LIBAEON_API udp_client_event_socket : public udp_event_socket {
public:
    udp_client_event_socket();
    udp_client_event_socket(const char* hostname, int port);
    ~udp_client_event_socket() override;

    bool connect(const char* hostname, int port);
    bool connect(const address& destination);
    bool connect();
};

} // namespace aeon
