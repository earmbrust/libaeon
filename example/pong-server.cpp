#include "pong_server.hpp"
#include <atomic>
#include <charconv>
#include <chrono>
#include <csignal>
#include <iostream>
#include <string_view>

namespace {
std::atomic<bool> exiting{false};
void stop_requested(int) { exiting.store(true); }
}

int main(int argc, char** argv) {
    if (argc > 2) { std::cerr << "Usage: pong-server [UDP port, default 2302]\n"; return 1; }
    int port = 2302;
    if (argc == 2) {
        const std::string_view s(argv[1]);
        const auto parsed = std::from_chars(s.data(), s.data() + s.size(), port);
        if (parsed.ec != std::errc{} || parsed.ptr != s.data() + s.size() ||
            port < 0 || port > 65535) { std::cerr << "Invalid port\n"; return 1; }
    }
    aeon_examples::pong_server game;
    if (!game.start(port)) { std::cerr << "Could not bind UDP port\n"; return 1; }
    std::signal(SIGINT, stop_requested);
    std::cout << "UDP Pong listening on port " << game.local_port() << " (Ctrl+C to stop)\n";
    auto prior = std::chrono::steady_clock::now();
    while (!exiting.load()) {
        if (game.poll(25) < 0) { std::cerr << "UDP poll failure\n"; return 1; }
        const auto now = std::chrono::steady_clock::now();
        game.advance(static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
            now - prior).count()));
        prior = now;
    }
    game.stop();
    return 0;
}
