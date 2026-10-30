#include "aeon.hpp"
#include "udp_event_socket.hpp"
#include <utility>


namespace aeon {
enum { handler_datagram, handler_connect, handler_close, handler_listen, handler_error };

udp_event_socket::udp_event_socket() : event_handlers(5) {
    if (is_valid_socket() && set_blocking(false) != 0) socket::close();
}
udp_event_socket::~udp_event_socket() = default;

void udp_event_socket::on_datagram(datagram_handler f) { set_handler(handler_datagram, f ? std::make_shared<const datagram_handler>(std::move(f)) : std::shared_ptr<const void>{}); }
void udp_event_socket::on_connect(notification_handler f) { set_handler(handler_connect, f ? std::make_shared<const notification_handler>(std::move(f)) : std::shared_ptr<const void>{}); }
void udp_event_socket::on_listen(notification_handler f) { set_handler(handler_listen, f ? std::make_shared<const notification_handler>(std::move(f)) : std::shared_ptr<const void>{}); }
void udp_event_socket::on_close(notification_handler f) { set_handler(handler_close, f ? std::make_shared<const notification_handler>(std::move(f)) : std::shared_ptr<const void>{}); }
void udp_event_socket::on_error(error_handler f) { set_handler(handler_error, f ? std::make_shared<const error_handler>(std::move(f)) : std::shared_ptr<const void>{}); }

void udp_event_socket::fire_on_datagram(const char* d, int n, const address& from) {
    register_sender(from);
    auto f = std::static_pointer_cast<const datagram_handler>(get_handler(handler_datagram));
    if (f) (*f)(d, n, from); else on_datagram_impl(d, n, from);
}
void udp_event_socket::fire_on_connect() {
    auto f = std::static_pointer_cast<const notification_handler>(get_handler(handler_connect));
    if (f) (*f)(); else on_connect_impl();
}
void udp_event_socket::fire_on_listen() {
    auto f = std::static_pointer_cast<const notification_handler>(get_handler(handler_listen));
    if (f) (*f)(); else on_listen_impl();
}
void udp_event_socket::fire_on_close() {
    auto f = std::static_pointer_cast<const notification_handler>(get_handler(handler_close));
    if (f) (*f)(); else on_close_impl();
}
void udp_event_socket::fire_on_error(int error) {
    auto f = std::static_pointer_cast<const error_handler>(get_handler(handler_error));
    if (f) (*f)(error); else on_error_impl(error);
}
bool udp_event_socket::close() {
    bool result = socket::close();
    if (result) fire_on_close();
    return result;
}
} // namespace aeon
