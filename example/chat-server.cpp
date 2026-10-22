#include "chat_server.hpp"

#include <atomic>
#include <charconv>
#include <csignal>
#include <iostream>
#include <string_view>

namespace {
std::atomic<bool> shutdown_requested{false};
void request_shutdown(int) { shutdown_requested.store(true); }
} // namespace

int main(int argc, char** argv) {
    if (argc > 3 || (argc > 1 && (std::string_view(argv[1]) == "-h" ||
                                   std::string_view(argv[1]) == "--help"))) {
        std::cout << "Usage: chat-server [port (default 2300)] [bind address (default 127.0.0.1)]\n";
        return argc > 3 ? 1 : 0;
    }
    int port = 2300;
    if (argc > 1) {
        const std::string_view arg(argv[1]);
        const auto parsed = std::from_chars(arg.data(), arg.data() + arg.size(), port);
        if (parsed.ec != std::errc{} || parsed.ptr != arg.data() + arg.size() ||
            port < 0 || port > 65535) {
            std::cerr << "Invalid port\n";
            return 1;
        }
    }
    const char* host = argc > 2 ? argv[2] : "127.0.0.1";
    aeon_examples::chat_server server;
    if (!server.start(host, port)) {
        std::cerr << "Unable to listen on " << host << ':' << port << '\n';
        return 1;
    }
    std::signal(SIGINT, request_shutdown);
    std::cout << "Chat listening at " << host << ':' << server.local_port()
              << " (Ctrl-C to stop)\n";
    while (!shutdown_requested.load()) {
        if (server.poll(100) < 0) {
            std::cerr << "Poll failure\n";
            server.stop();
            return 1;
        }
    }
    server.stop();
    return 0;
}
