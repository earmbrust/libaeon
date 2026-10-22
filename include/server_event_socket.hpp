#pragma once

#include "event_socket.hpp"
#include <memory>

namespace aeon {

class LIBAEON_API server_event_socket : public event_socket {
public:
    server_event_socket();
    explicit server_event_socket(int port);
    virtual ~server_event_socket();

    int set_accept_timeout(int timeout_ms);
    int bind(const sockaddr* addr, socklen_t len);
    bool listen();
    bool listen(int port);
    bool listen(const char* address, int port);
    std::unique_ptr<event_socket> accept();
    std::unique_ptr<event_socket> accept(bool blocking);
    bool close() override;

protected:
    void on_data_impl(const char* buffer, int size) override;
    void on_listen_impl() override;
    void on_close_impl() override;
    void on_error_impl(int error_code) override;

private:
    int accept_timeout_ms_ = 0;
};

} // namespace aeon
