#pragma once

#include "event_socket.hpp"

namespace aeon {

class LIBAEON_API client_event_socket : public event_socket {
public:
    client_event_socket();
    explicit client_event_socket(const char* hostname, int port);
    virtual ~client_event_socket();

    bool connect();
    bool connect(const char* hostname, int port);
    bool connect(const address& addr);
    bool close() override;

private:
    bool finish_connect();

};

} // namespace aeon
