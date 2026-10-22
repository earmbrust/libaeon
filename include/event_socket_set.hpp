/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/
#pragma once

#include "event_socket.hpp"
#include <functional>

namespace aeon {
namespace internal { struct set_callback_state; }

class LIBAEON_API event_socket_set {
public:
    using data_handler = std::function<void(event_socket*, const char*, int)>;
    using notification_handler = std::function<void(event_socket*)>;
    using error_handler = std::function<void(event_socket*, int)>;

    event_socket_set();
    virtual ~event_socket_set();
    event_socket_set(const event_socket_set&) = delete;
    event_socket_set& operator=(const event_socket_set&) = delete;

    // Non-owning registration. Deregister before destroying a socket. Removal
    // invalidates pending poll snapshots but does not wait for an in-progress
    // callback. Socket destruction cannot race active event dispatch.
    bool add(event_socket* socket);
    bool remove(unsigned int index);
    bool remove_socket(event_socket* socket);
    int size() const;

    // Callbacks execute synchronously on the thread performing dispatch.
    void on_socket_data(data_handler handler);
    void on_socket_connect(notification_handler handler);
    void on_socket_close(notification_handler handler);
    void on_socket_listen(notification_handler handler);
    void on_socket_error(error_handler handler);

protected:
    virtual void on_socket_data_impl(event_socket*, const char*, int) {}
    virtual void on_socket_connect_impl(event_socket*) {}
    virtual void on_socket_close_impl(event_socket*) {}
    virtual void on_socket_listen_impl(event_socket*) {}
    virtual void on_socket_error_impl(event_socket*, int) {}

    void fire_on_socket_data(event_socket* socket, const char* buffer, int size);
    void fire_on_socket_connect(event_socket* socket);
    void fire_on_socket_close(event_socket* socket);
    void fire_on_socket_listen(event_socket* socket);
    void fire_on_socket_error(event_socket* socket, int error_code);

private:
    internal::set_callback_state* state_;
};
} // namespace aeon
