/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/
#pragma once

#include "socket.hpp"
#include "internal/event_handlers.hpp"
#include <functional>
#include <cstddef>

namespace aeon {
namespace internal { struct pending_write_state; }

class LIBAEON_API event_socket : public socket, protected internal::event_handlers {
    friend class event_socket_set;
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

    // Opt-in buffered send for nonblocking TCP streams. Accepted bytes are
    // copied, never sent inline. poll() flushes at most 64 KiB per socket per
    // iteration; call it again while queued_bytes() is nonzero. Returns false
    // without accepting any bytes if the queue would exceed its bound.
    // Existing write() overloads retain their direct, partial-send semantics.
    // Do not mix direct write() with queued writes on the same stream unless
    // the caller coordinates ordering: direct writes bypass pending output.
    // Drain notifications mean the queue reached empty during a poll flush;
    // a concurrent producer can enqueue more data before notification.
    bool queue_write(const char* data, std::size_t size);
    bool queue_write(const std::string& data);
    std::size_t queued_bytes() const;
    // Rejects a new limit smaller than the currently queued byte count.
    bool set_max_queued_bytes(std::size_t limit);
    // Called once when a nonempty queue is completely transmitted.
    void on_drain(notification_handler handler);

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
    void fire_on_drain();

private:
    struct flush_result { std::size_t bytes = 0; int error = 0; bool drained = false; };
    flush_result flush_queued(std::size_t budget);



private:
    internal::pending_write_state* outgoing_;
};
} // namespace aeon
