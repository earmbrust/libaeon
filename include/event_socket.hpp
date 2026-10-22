/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/
#pragma once

#include "socket.hpp"
#include <functional>

namespace aeon {
namespace internal { struct socket_callback_state; }

class LIBAEON_API event_socket : public socket {
public:
    using data_handler = std::function<void(const char*, int)>;
    using notification_handler = std::function<void()>;
    using error_handler = std::function<void(int)>;

    event_socket();
    ~event_socket() override;
    using socket::write;

    int write(const char* data) override;
    int write(const std::string& data) override;
    bool close() override;

    // One handler per event. Replacing or clearing a handler is synchronized.
    // Dispatch invokes the snapshot outside the registration lock, on the
    // calling thread. Socket destruction must not race with dispatch.
    void on_data(data_handler handler);
    void on_connect(notification_handler handler);
    void on_close(notification_handler handler);
    void on_listen(notification_handler handler);
    void on_error(error_handler handler);

protected:
    explicit event_socket(socket_t existing_fd, bool adopt);
    virtual void on_data_impl(const char* buffer, int size);
    virtual void on_connect_impl();
    virtual void on_close_impl();
    virtual void on_listen_impl();
    virtual void on_error_impl(int error_code);

    void fire_on_data(const char* buffer, int size);
    void fire_on_connect();
    void fire_on_close();
    void fire_on_listen();
    void fire_on_error(int error_code);

private:
    internal::socket_callback_state* callbacks_;
};
} // namespace aeon
