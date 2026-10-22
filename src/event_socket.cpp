/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/
#include "event_socket.hpp"
#include "internal/callback_state.hpp"
#include <cstring>
#include <utility>

namespace aeon {

event_socket::event_socket() : socket(), callbacks_(new internal::socket_callback_state) {
    if (is_valid_socket() && set_blocking(false) != 0) socket::close();
}

event_socket::event_socket(socket_t fd, bool adopt)
    : socket(fd, adopt), callbacks_(new internal::socket_callback_state) {
    if (is_valid_socket() && set_blocking(false) != 0) socket::close();
}

event_socket::~event_socket() { delete callbacks_; }

void event_socket::on_data(data_handler f) { callbacks_->data.set(std::move(f)); }
void event_socket::on_connect(notification_handler f) { callbacks_->connect.set(std::move(f)); }
void event_socket::on_close(notification_handler f) { callbacks_->close.set(std::move(f)); }
void event_socket::on_listen(notification_handler f) { callbacks_->listen.set(std::move(f)); }
void event_socket::on_error(error_handler f) { callbacks_->error.set(std::move(f)); }

void event_socket::fire_on_data(const char* data, int size) {
    auto handler = callbacks_->data.snapshot();
    if (handler) (*handler)(data, size);
    else on_data_impl(data, size);
}
void event_socket::fire_on_connect() {
    auto handler = callbacks_->connect.snapshot();
    if (handler) (*handler)();
    else on_connect_impl();
}
void event_socket::fire_on_close() {
    auto handler = callbacks_->close.snapshot();
    if (handler) (*handler)();
    else on_close_impl();
}
void event_socket::fire_on_listen() {
    auto handler = callbacks_->listen.snapshot();
    if (handler) (*handler)();
    else on_listen_impl();
}
void event_socket::fire_on_error(int error) {
    auto handler = callbacks_->error.snapshot();
    if (handler) (*handler)(error);
    else on_error_impl(error);
}

void event_socket::on_data_impl(const char*, int) {}
void event_socket::on_connect_impl() {}
void event_socket::on_close_impl() {}
void event_socket::on_listen_impl() {}
void event_socket::on_error_impl(int) {}

bool event_socket::close() {
    bool closed = socket::close();
    if (closed) fire_on_close();
    return closed;
}

int event_socket::write(const char* data) {
    if (!data || !is_valid_socket()) return NET_SOCKET_ERROR;
    auto len = std::strlen(data);
    if (len == 0) return 0;
    const int sent = static_cast<int>(send(sockfd, data, static_cast<int>(len), 0));
    if (sent < 0) {
        error_code = GET_NET_SOCKET_ERROR();
        fire_on_error(error_code);
    }
    return sent;
}

int event_socket::write(const std::string& data) {
    if (!is_valid_socket() || data.empty()) return NET_SOCKET_ERROR;
    const int sent = static_cast<int>(send(sockfd, data.data(),
                                           static_cast<int>(data.size()), 0));
    if (sent < 0) {
        error_code = GET_NET_SOCKET_ERROR();
        fire_on_error(error_code);
    }
    return sent;
}

} // namespace aeon
