#pragma once

#include "aeon.hpp"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace aeon_examples {

struct chat_limits {
    std::size_t max_clients = 128;
    std::size_t max_line_bytes = 1024;
    std::size_t max_queued_bytes = 128 * 1024;
};

// A single-threaded, application-owned, newline-delimited TCP chat server.
// The server must not be moved, destroyed or stopped from within poll().
class chat_server {
public:
    explicit chat_server(chat_limits limits = {});
    ~chat_server();
    chat_server(const chat_server&) = delete;
    chat_server& operator=(const chat_server&) = delete;

    bool start(const char* bind_address = "127.0.0.1", int port = 2300);
    int poll(int timeout_ms);
    void stop();
    bool running() const noexcept { return running_; }
    int local_port() const;
    std::size_t client_count() const;

private:
    struct peer {
        std::unique_ptr<aeon::event_socket> socket;
        std::string pending_line;
        std::uint64_t id = 0;
        bool retire = false;
    };

    void accept_ready();
    void receive(aeon::event_socket*, const char*, int);
    void broadcast(const peer&, const std::string&);
    void retire(aeon::event_socket*);
    void reap();
    peer* lookup(aeon::event_socket*);

    chat_limits limits_;
    aeon::server_event_socket listener_;
    aeon::event_socket_set sockets_;
    std::vector<std::unique_ptr<peer>> peers_;
    std::uint64_t next_id_ = 1;
    bool running_ = false;
    bool polling_ = false;
};

} // namespace aeon_examples
