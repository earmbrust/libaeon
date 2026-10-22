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

protected:
    void on_data_impl(const char* buffer, int size) override;
    void on_connect_impl() override;
    void on_close_impl() override;
    void on_error_impl(int error_code) override;
};

} // namespace aeon
