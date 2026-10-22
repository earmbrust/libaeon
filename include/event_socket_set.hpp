/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/
#pragma once

#include "event_socket.hpp"
#include "udp_event_socket.hpp"
#include <functional>
#include <cstddef>

namespace aeon {
namespace internal { struct set_callback_state; }

class LIBAEON_API event_socket_set {
public:
    using data_handler = std::function<void(event_socket*, const char*, int)>;
    using notification_handler = std::function<void(event_socket*)>;
    using error_handler = std::function<void(event_socket*, int)>;
    using datagram_handler = std::function<void(udp_event_socket*, const char*, int, const address&)>;
    using udp_error_handler = std::function<void(udp_event_socket*, int)>;

    event_socket_set();
    virtual ~event_socket_set();
    event_socket_set(const event_socket_set&) = delete;
    event_socket_set& operator=(const event_socket_set&) = delete;

    // Non-owning registration. Deregister before destroying a socket. Removal
    // invalidates pending poll snapshots but does not wait for an in-progress
    // callback. Socket destruction cannot race active event dispatch.
    bool add(event_socket* socket);
    bool add(udp_event_socket* socket);
    bool add(std::nullptr_t) noexcept { return false; }
    bool remove(unsigned int index);
    bool remove_socket(event_socket* socket);
    bool remove_udp_socket(udp_event_socket* socket);
    // Returns the total number of TCP and UDP registrations. remove(index)
    // indexes TCP registrations first, followed by UDP registrations.
    int size() const;

    // Application-driven readiness dispatch. No internal threads.
    // timeout_ms: -1 waits indefinitely when sockets exist; 0 is nonblocking;
    // positive values wait for at most that many milliseconds.
    // Returns readiness events handled, 0 for timeout/no active sockets, -1 on
    // invalid timeout, concurrent/reentrant poll, or native poll failure.
    // Exceptions from user callbacks propagate. Concurrent destruction of
    // registered sockets or of this set during poll is not supported.
    int poll(int timeout_ms);

    // Callbacks execute synchronously on the thread performing dispatch.
    void on_socket_data(data_handler handler);
    void on_socket_connect(notification_handler handler);
    void on_socket_close(notification_handler handler);
    // For registered TCP listeners, this signals pending accept readiness.
    // server_event_socket::on_listen signals successful listen() separately.
    void on_socket_listen(notification_handler handler);
    void on_socket_error(error_handler handler);
    void on_socket_datagram(datagram_handler handler);
    void on_udp_socket_error(udp_error_handler handler);

protected:
    virtual void on_socket_data_impl(event_socket*, const char*, int) {}
    virtual void on_socket_connect_impl(event_socket*) {}
    virtual void on_socket_close_impl(event_socket*) {}
    virtual void on_socket_listen_impl(event_socket*) {}
    virtual void on_socket_error_impl(event_socket*, int) {}
    virtual void on_socket_datagram_impl(udp_event_socket*, const char*, int, const address&) {}
    virtual void on_udp_socket_error_impl(udp_event_socket*, int) {}

    void fire_on_socket_data(event_socket* socket, const char* buffer, int size);
    void fire_on_socket_connect(event_socket* socket);
    void fire_on_socket_close(event_socket* socket);
    void fire_on_socket_listen(event_socket* socket);
    void fire_on_socket_error(event_socket* socket, int error_code);
    void fire_on_socket_datagram(udp_event_socket*, const char*, int, const address&);
    void fire_on_udp_socket_error(udp_event_socket*, int);

private:
    internal::set_callback_state* state_;
};
} // namespace aeon
