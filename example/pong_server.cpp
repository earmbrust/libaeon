#include "aeon.hpp"
#include "pong_server.hpp"
#include <algorithm>
#include <cstring>
#include <string>

namespace aeon_examples {
namespace {

bool same_endpoint(const aeon::address& a, const aeon::address& b) {
    const auto lhs = a.get_sockaddr_storage();
    const auto rhs = b.get_sockaddr_storage();
    if (lhs.ss_family != rhs.ss_family || a.get_port() != b.get_port()) return false;
    if (lhs.ss_family == AF_INET) {
        return reinterpret_cast<const sockaddr_in*>(&lhs)->sin_addr.s_addr ==
               reinterpret_cast<const sockaddr_in*>(&rhs)->sin_addr.s_addr;
    }
    if (lhs.ss_family == AF_INET6) {
        const auto* l = reinterpret_cast<const sockaddr_in6*>(&lhs);
        const auto* r = reinterpret_cast<const sockaddr_in6*>(&rhs);
        return l->sin6_scope_id == r->sin6_scope_id &&
               std::memcmp(&l->sin6_addr, &r->sin6_addr, sizeof(l->sin6_addr)) == 0;
    }
    return false;
}

} // namespace

pong_server::pong_server() {
    events_.on_socket_datagram([this](aeon::udp_event_socket* s, const char* bytes,
                                     int n, const aeon::address& from) {
        if (s == &server_) datagram(bytes, n, from);
    });
}

pong_server::~pong_server() { stop(); }

bool pong_server::start(int port) {
    if (running_ || polling_ || port < 0 || port > 65535 || !server_.listen(port)) return false;
    if (!events_.add(&server_)) {
        server_.close();
        return false;
    }
    running_ = true;
    return true;
}

int pong_server::local_port() const {
    if (!running_) return -1;
    sockaddr_storage local{};
    socklen_t len = static_cast<socklen_t>(sizeof(local));
    if (::getsockname(server_.sockfd, reinterpret_cast<sockaddr*>(&local), &len) != 0)
        return -1;
    return local.ss_family == AF_INET6
        ? ntohs(reinterpret_cast<const sockaddr_in6*>(&local)->sin6_port)
        : ntohs(reinterpret_cast<const sockaddr_in*>(&local)->sin_port);
}

int pong_server::player_count() const {
    return static_cast<int>(players_[0].active) + static_cast<int>(players_[1].active);
}

pong_server::state pong_server::current() const {
    auto s = state_;
    s.occupied = (players_[0].active ? 1 : 0) | (players_[1].active ? 2 : 0);
    return s;
}

int pong_server::seat(const aeon::address& from) const {
    for (int i = 0; i < 2; ++i)
        if (players_[i].active && same_endpoint(players_[i].endpoint, from)) return i;
    return -1;
}

void pong_server::reply(const aeon::address& to, std::string_view payload) {
    if (auto* peer = server_.get_peer(to))
        peer->write(payload.data(), static_cast<int>(payload.size()));
}

void pong_server::datagram(const char* bytes, int count, const aeon::address& from) {
    if (!running_ || !bytes || count <= 0 || count > 32) return;
    const std::string_view cmd(bytes, static_cast<std::size_t>(count));
    int who = seat(from);
    if (cmd == "J") {
        if (who < 0) {
            for (int i = 0; i < 2; ++i) {
                if (!players_[i].active) {
                    who = i;
                    players_[i] = {true, from, 0, 0};
                    break;
                }
            }
        }
        if (who < 0) reply(from, "FULL");
        else {
            players_[who].inactivity_ms = 0;
            reply(from, who == 0 ? "W 1" : "W 2");
            snapshot();
        }
        return;
    }
    // Unknown sources cannot issue paddle input, reset scores, or leave.
    if (who < 0) return;
    auto& p = players_[who];
    if (cmd == "Q") {
        p = {};
        reset_ball(1);
        snapshot();
        return;
    }
    if (cmd == "H" || cmd == "U" || cmd == "D" || cmd == "N") {
        p.inactivity_ms = 0;
        if (cmd == "U") p.direction = -1;
        if (cmd == "D") p.direction = 1;
        if (cmd == "N") p.direction = 0;
    }
}

void pong_server::reset_ball(int direction) {
    state_.ball_x = width / 2;
    state_.ball_y = height / 2;
    dx_ = direction;
    dy_ = 1;
}

void pong_server::step() {
    for (auto& p : players_) {
        if (!p.active) continue;
        p.inactivity_ms += tick_ms;
        if (p.inactivity_ms >= idle_timeout_ms) {
            p = {};
            reset_ball(1);
        }
    }
    if (player_count() == 2) {
        state_.left_y = std::clamp(state_.left_y + players_[0].direction, 0,
                                   height - paddle_height);
        state_.right_y = std::clamp(state_.right_y + players_[1].direction, 0,
                                    height - paddle_height);
        int next_y = state_.ball_y + dy_;
        if (next_y < 0 || next_y >= height) {
            dy_ = -dy_;
            next_y = state_.ball_y + dy_;
        }
        int next_x = state_.ball_x + dx_;
        if (next_x == 2 && dx_ < 0 && next_y >= state_.left_y &&
            next_y < state_.left_y + paddle_height) {
            dx_ = 1;
            next_x = state_.ball_x + dx_;
        } else if (next_x == width - 3 && dx_ > 0 && next_y >= state_.right_y &&
                   next_y < state_.right_y + paddle_height) {
            dx_ = -1;
            next_x = state_.ball_x + dx_;
        }
        state_.ball_x = next_x;
        state_.ball_y = next_y;
        if (next_x <= 0) {
            ++state_.right_score;
            reset_ball(1);
        } else if (next_x >= width - 1) {
            ++state_.left_score;
            reset_ball(-1);
        }
    }
    snapshot();
}

void pong_server::snapshot() {
    const state s = current();
    const std::string frame = "S " + std::to_string(++state_.sequence) + " " +
        std::to_string(s.left_y) + " " + std::to_string(s.right_y) + " " +
        std::to_string(s.ball_x) + " " + std::to_string(s.ball_y) + " " +
        std::to_string(s.left_score) + " " + std::to_string(s.right_score) + " " +
        std::to_string(s.occupied);
    for (const auto& p : players_)
        if (p.active) reply(p.endpoint, frame);
}

int pong_server::poll(int timeout_ms) {
    if (!running_ || polling_) return -1;
    polling_ = true;
    try {
        const int result = events_.poll(timeout_ms);
        polling_ = false;
        return result;
    } catch (...) {
        polling_ = false;
        throw;
    }
}

void pong_server::advance(int elapsed_ms) {
    if (!running_ || polling_ || elapsed_ms <= 0) return;
    // Cap catch-up to avoid a long pause causing thousands of bursts.
    accumulator_ms_ += std::min(elapsed_ms, tick_ms * 4);
    while (accumulator_ms_ >= tick_ms) {
        accumulator_ms_ -= tick_ms;
        step();
    }
}

void pong_server::stop() {
    if (polling_) return; // Called outside poll()/callbacks only.
    events_.remove_udp_socket(&server_);
    server_.close();
    players_[0] = {};
    players_[1] = {};
    running_ = false;
    accumulator_ms_ = 0;
}

} // namespace aeon_examples
