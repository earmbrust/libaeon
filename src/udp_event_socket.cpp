#include "udp_event_socket.hpp"
#include "internal/callback_state.hpp"
#include <utility>

namespace aeon::internal {

struct udp_callback_state {
    callback_slot<void(const char*, int, const address&)> datagram;
    callback_slot<void()> connect;
    callback_slot<void()> close;
    callback_slot<void()> listen;
    callback_slot<void(int)> error;
};

} // namespace aeon::internal

namespace aeon {
udp_event_socket::udp_event_socket() : callbacks_(new internal::udp_callback_state) {
    if (is_valid_socket() && set_blocking(false) != 0) socket::close();
}
udp_event_socket::~udp_event_socket() { delete callbacks_; }

void udp_event_socket::on_datagram(datagram_handler f) { callbacks_->datagram.set(std::move(f)); }
void udp_event_socket::on_connect(notification_handler f) { callbacks_->connect.set(std::move(f)); }
void udp_event_socket::on_listen(notification_handler f) { callbacks_->listen.set(std::move(f)); }
void udp_event_socket::on_close(notification_handler f) { callbacks_->close.set(std::move(f)); }
void udp_event_socket::on_error(error_handler f) { callbacks_->error.set(std::move(f)); }

void udp_event_socket::fire_on_datagram(const char* d, int n, const address& from) {
    register_sender(from);
    auto f = callbacks_->datagram.snapshot();
    if (f) (*f)(d, n, from); else on_datagram_impl(d, n, from);
}
void udp_event_socket::fire_on_connect() {
    auto f = callbacks_->connect.snapshot();
    if (f) (*f)(); else on_connect_impl();
}
void udp_event_socket::fire_on_listen() {
    auto f = callbacks_->listen.snapshot();
    if (f) (*f)(); else on_listen_impl();
}
void udp_event_socket::fire_on_close() {
    auto f = callbacks_->close.snapshot();
    if (f) (*f)(); else on_close_impl();
}
void udp_event_socket::fire_on_error(int error) {
    auto f = callbacks_->error.snapshot();
    if (f) (*f)(error); else on_error_impl(error);
}
bool udp_event_socket::close() {
    bool result = socket::close();
    if (result) fire_on_close();
    return result;
}
} // namespace aeon
