#pragma once

#include "aeon.hpp"
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace aeon_examples {

// Application-driven UDP Pong. Call poll() for network input and advance()
// with elapsed wall-clock time for simulation. No application worker threads.
class pong_server {
public:
    static constexpr int width = 24;
    static constexpr int height = 12;
    static constexpr int paddle_height = 3;
    static constexpr int tick_ms = 100;
    static constexpr int idle_timeout_ms = 5000;

    struct state {
        int left_y = 4;
        int right_y = 4;
        int ball_x = width / 2;
        int ball_y = height / 2;
        int left_score = 0;
        int right_score = 0;
        int occupied = 0;
        std::uint64_t sequence = 0;
    };

    pong_server();
    ~pong_server();
    pong_server(const pong_server&) = delete;
    pong_server& operator=(const pong_server&) = delete;

    bool start(int port = 2302);
    int poll(int timeout_ms);
    void advance(int elapsed_ms); // bounded work even if a caller stalls
    void stop();
    int local_port() const;
    int player_count() const;
    state current() const;

private:
    struct player {
        bool active = false;
        aeon::address endpoint;
        int direction = 0;
        int inactivity_ms = 0;
    };
    void datagram(const char* bytes, int count, const aeon::address& from);
    int seat(const aeon::address& from) const;
    void reply(const aeon::address& to, std::string_view payload);
    void reset_ball(int direction);
    void step();
    void snapshot();

    aeon::udp_server_event_socket server_;
    aeon::event_socket_set events_;
    player players_[2];
    state state_;
    int dx_ = 1;
    int dy_ = 1;
    int accumulator_ms_ = 0;
    bool running_ = false;
    bool polling_ = false;
};

} // namespace aeon_examples
