/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/
#include "event_socket_set.hpp"
#include "internal/callback_state.hpp"
#include <utility>

namespace aeon {
event_socket_set::event_socket_set() : state_(new internal::set_callback_state) {}
event_socket_set::~event_socket_set() { delete state_; }

bool event_socket_set::add(event_socket* socket) {
    return state_->sockets.add(socket);
}
bool event_socket_set::remove(unsigned int index) {
    return state_->sockets.remove_index(index);
}
bool event_socket_set::remove_socket(event_socket* socket) {
    return state_->sockets.remove_socket(socket);
}
int event_socket_set::size() const {
    return state_->sockets.size();
}

void event_socket_set::on_socket_data(data_handler f) { state_->data.set(std::move(f)); }
void event_socket_set::on_socket_connect(notification_handler f) { state_->connect.set(std::move(f)); }
void event_socket_set::on_socket_close(notification_handler f) { state_->close.set(std::move(f)); }
void event_socket_set::on_socket_listen(notification_handler f) { state_->listen.set(std::move(f)); }
void event_socket_set::on_socket_error(error_handler f) { state_->error.set(std::move(f)); }

void event_socket_set::fire_on_socket_data(event_socket* s, const char* d, int n) {
    auto f = state_->data.snapshot();
    if (f) (*f)(s, d, n); else on_socket_data_impl(s, d, n);
}
void event_socket_set::fire_on_socket_connect(event_socket* s) {
    auto f = state_->connect.snapshot();
    if (f) (*f)(s); else on_socket_connect_impl(s);
}
void event_socket_set::fire_on_socket_close(event_socket* s) {
    auto f = state_->close.snapshot();
    if (f) (*f)(s); else on_socket_close_impl(s);
}
void event_socket_set::fire_on_socket_listen(event_socket* s) {
    auto f = state_->listen.snapshot();
    if (f) (*f)(s); else on_socket_listen_impl(s);
}
void event_socket_set::fire_on_socket_error(event_socket* s, int code) {
    auto f = state_->error.snapshot();
    if (f) (*f)(s, code); else on_socket_error_impl(s, code);
}
} // namespace aeon
