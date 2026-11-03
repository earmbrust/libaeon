#pragma once

#include "aeon.hpp"
#include <chrono>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace aeon_examples {

struct http_limits {
    std::size_t max_clients = 128;
    std::size_t max_header_bytes = 8192;
    std::size_t max_queued_bytes = 128 * 1024;
    int idle_timeout_ms = 30000;
};

// An intentionally small plaintext HTTP/1.x example, not a general-purpose
// internet-facing HTTP implementation. Application drives poll() itself.
// Stop/destroy only outside poll(); sockets are owned by this application.
class http_server {
public:
    explicit http_server(http_limits limits = {});
    ~http_server();
    http_server(const http_server&) = delete;
    http_server& operator=(const http_server&) = delete;

    bool start(const char* bind_address = "127.0.0.1", int port = 8080);
    int poll(int timeout_ms);
    void stop();
    bool running() const noexcept { return running_; }
    int local_port() const;
    std::size_t client_count() const;

private:
    using clock = std::chrono::steady_clock;
    struct peer {
        aeon::event_socket* socket = nullptr; // owned by listener_
        std::string incoming;
        clock::time_point last_activity;
        bool closing = false;
        bool retire = false;
    };

    void accept_ready();
    void receive(aeon::event_socket*, const char*, int);
    void drained(aeon::event_socket*);
    void retire(aeon::event_socket*);
    void reap();
    peer* lookup(aeon::event_socket*);
    bool send_response(peer&, int status, const std::string& content_type,
                       const std::string& body, bool head, bool close,
                       bool allow_header = false, bool http10 = false);
    void handle_request(peer&, const std::string& request);

    http_limits limits_;
    aeon::server_event_socket listener_;
    aeon::event_socket_set sockets_;
    std::vector<std::unique_ptr<peer>> peers_;
    bool running_ = false;
    bool polling_ = false;
};

} // namespace aeon_examples
