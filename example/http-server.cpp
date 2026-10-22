#include "http_server.hpp"

#include <atomic>
#include <charconv>
#include <csignal>
#include <iostream>
#include <string_view>

namespace {
std::atomic<bool> shutdown_requested{false};
void request_shutdown(int) { shutdown_requested.store(true); }
}

int main(int argc, char** argv) {
    if (argc > 3 || (argc > 1 && (std::string_view(argv[1]) == "-h" ||
                                   std::string_view(argv[1]) == "--help"))) {
        std::cout << "Usage: http-server [port (default 8080)] [bind address (default 127.0.0.1)]\n";
        return argc > 3 ? 1 : 0;
    }
    int port = 8080;
    if (argc > 1) {
        const std::string_view value(argv[1]);
        const auto parsed = std::from_chars(value.data(), value.data() + value.size(), port);
        if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() ||
            port < 0 || port > 65535) {
            std::cerr << "Invalid port\n";
            return 1;
        }
    }
    const char* host = argc > 2 ? argv[2] : "127.0.0.1";
    aeon_examples::http_server server;
    if (!server.start(host, port)) {
        std::cerr << "Cannot listen on " << host << ':' << port << '\n';
        return 1;
    }
    std::signal(SIGINT, request_shutdown);
    std::cout << "HTTP listening at " << host << ':' << server.local_port()
              << " (Ctrl-C to stop)\n";
    while (!shutdown_requested.load()) {
        if (server.poll(100) < 0) {
            std::cerr << "HTTP poll failure\n";
            server.stop();
            return 1;
        }
    }
    server.stop();
}
