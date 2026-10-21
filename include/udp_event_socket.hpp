#pragma once

#include "udp_socket.hpp"
#include <functional>

namespace aeon {

class LIBAEON_API udp_event_socket : public udp_socket, protected internal::event_handlers {
    friend class event_socket_set;
public:
    using datagram_handler = std::function<void(const char*, int, const address&)>;
    using notification_handler = std::function<void()>;
    using error_handler = std::function<void(int)>;

    udp_event_socket();
    ~udp_event_socket() override;
    bool close() override;

    void on_datagram(datagram_handler handler);
    void on_connect(notification_handler handler);
    void on_listen(notification_handler handler);
    void on_close(notification_handler handler);
    void on_error(error_handler handler);

protected:
    // Hook for server subclasses to register a peer before user callbacks.
    virtual void register_sender(const address&) {}
    virtual void on_datagram_impl(const char*, int, const address&) {}
    virtual void on_connect_impl() {}
    virtual void on_listen_impl() {}
    virtual void on_close_impl() {}
    virtual void on_error_impl(int) {}

    void fire_on_datagram(const char*, int, const address&);
    void fire_on_connect();
    void fire_on_listen();
    void fire_on_close();
    void fire_on_error(int);

private:

};
} // namespace aeon
