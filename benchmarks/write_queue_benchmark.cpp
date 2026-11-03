// Standalone loopback benchmark: no external benchmark framework required.
// Reports measured throughput, not a pass/fail performance threshold.
#include "aeon.hpp"
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>

int main(int argc, char** argv) {
    const auto total = argc > 1 ? std::strtoull(argv[1], nullptr, 10) : 32ULL*1024*1024;
    const auto chunk = argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 4096ULL;
    if (!total || !chunk || chunk > 64*1024) {
        std::cerr << "Usage: aeon_write_queue_benchmark [total_bytes=33554432] [chunk_bytes=4096 (1..65536)]\n";
        return 2;
    }
    aeon::server_event_socket listener;
    if (!listener.listen("127.0.0.1", 0)) return 1;
    sockaddr_in bound{};
    socklen_t length = sizeof(bound);
    if (::getsockname(listener.sockfd, reinterpret_cast<sockaddr*>(&bound), &length) != 0) return 1;
    aeon::client_socket receiver;
    if (!receiver.connect("127.0.0.1", ntohs(bound.sin_port))) return 1;
    auto writer = listener.accept(false);
    if (!writer || receiver.set_blocking(false) != 0) return 1;
    aeon::event_socket_set set;
    if (!set.add(writer)) return 1;
    if (!writer->set_max_queued_bytes(1024*1024)) return 1;

    std::string payload(static_cast<std::size_t>(chunk), 'q');
    char sink[64*1024];
    unsigned long long submitted = 0, delivered = 0, queue_rejections = 0;
    const auto start = std::chrono::steady_clock::now();
    while (delivered < total) {
        while (submitted < total) {
            const auto n = static_cast<std::size_t>(std::min<unsigned long long>(chunk, total - submitted));
            if (!writer->queue_write(payload.data(), n)) { ++queue_rejections; break; }
            submitted += n;
        }
        if (set.poll(1) < 0) { std::cerr << "poll failed\n"; return 1; }
        for (;;) {
            const int n = static_cast<int>(::recv(receiver.sockfd, sink, sizeof(sink), 0));
            if (n > 0) {
                for (int i=0;i<n;++i) if (sink[i] != 'q') { std::cerr << "data mismatch\n"; return 1; }
                delivered += static_cast<unsigned long long>(n);
            } else if (n == 0) { std::cerr << "premature EOF\n"; return 1; }
            else {
                const int e = GET_NET_SOCKET_ERROR();
                #ifdef NET_PLATFORM_WINDOWS
                if (e == WSAEWOULDBLOCK || e == WSAEINTR) break;
#else
                if (e == EAGAIN || e == EWOULDBLOCK || e == EINTR) break;
#endif
                std::cerr << "recv failed: " << e << '\n'; return 1;
            }
        }
        if (std::chrono::steady_clock::now() - start > std::chrono::seconds(30)) {
            std::cerr << "benchmark timed out\n"; return 1;
        }
    }
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    std::cout << "bytes=" << delivered << " chunk=" << chunk << " seconds="
              << std::fixed << std::setprecision(4) << seconds << " MiB/s="
              << std::setprecision(2) << (delivered / (1024.0*1024.0) / seconds)
              << " queue_full_retries=" << queue_rejections << '\n';
    set.remove_socket(writer);
    return submitted == delivered ? 0 : 1;
}
