#include "aeon.hpp"
#include <charconv>
#include <csignal>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#ifdef NET_PLATFORM_WINDOWS
#include <conio.h>
#include <windows.h>
#else
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace {
volatile std::sig_atomic_t interruption_requested = 0;
void interrupt_client(int) { interruption_requested = 1; }
#ifdef NET_PLATFORM_WINDOWS
struct terminal {
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD original = 0;
    bool restore = false;
    terminal() {
        if (GetConsoleMode(out, &original)) {
            restore = SetConsoleMode(out, original | ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
        }
    }
    ~terminal() { if (restore) SetConsoleMode(out, original); }
    int key() { return _kbhit() ? _getch() : -1; }
};
#else
struct terminal {
    termios original{};
    bool restore = false;
    terminal() {
        if (isatty(STDIN_FILENO) && tcgetattr(STDIN_FILENO, &original) == 0) {
            termios raw = original;
            raw.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO));
            raw.c_cc[VMIN] = 0;
            raw.c_cc[VTIME] = 0;
            restore = tcsetattr(STDIN_FILENO, TCSANOW, &raw) == 0;
        }
    }
    ~terminal() { if (restore) tcsetattr(STDIN_FILENO, TCSANOW, &original); }
    int key() {
        fd_set fds; FD_ZERO(&fds); FD_SET(STDIN_FILENO, &fds);
        timeval tv{};
        if (select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv) <= 0) return -1;
        unsigned char c = 0;
        return ::read(STDIN_FILENO, &c, 1) == 1 ? c : -1;
    }
};
#endif
struct view {
    std::uint64_t sequence = 0;
    int ly = 4, ry = 4, x = 12, y = 6, score1 = 0, score2 = 0, occupied = 0;
};

bool parse_state(std::string_view packet, view& out) {
    if (packet.size() > 128 || packet.substr(0, 2) != "S ") return false;
    std::istringstream in{std::string(packet)};
    char tag = 0;
    view s{};
    if (!(in >> tag >> s.sequence >> s.ly >> s.ry >> s.x >> s.y >>
          s.score1 >> s.score2 >> s.occupied)) return false;
    std::string trailing;
    if (tag != 'S' || (in >> trailing) || s.ly < 0 || s.ry < 0 ||
        s.ly > 9 || s.ry > 9 || s.x < 0 || s.x >= 24 ||
        s.y < 0 || s.y >= 12 || s.score1 < 0 || s.score2 < 0 ||
        s.occupied < 0 || s.occupied > 3) return false;
    if (s.sequence <= out.sequence) return false;
    out = s;
    return true;
}

void draw(const view& s, int player) {
    std::cout << "\x1b[H\x1b[2JUDP PONG    " << s.score1 << " : " << s.score2
              << "    You: player " << player << "\n"
              << "W = up  S = down  X = stop  Q = quit\n"
              << ((s.occupied == 3) ? "Match in progress" : "Waiting for second player")
              << "\n+------------------------+\n";
    for (int y = 0; y < 12; ++y) {
        std::cout << '|';
        for (int x = 0; x < 24; ++x) {
            char c = ' ';
            if (x == 1 && (s.occupied & 1) && y >= s.ly && y < s.ly + 3) c = '#';
            if (x == 22 && (s.occupied & 2) && y >= s.ry && y < s.ry + 3) c = '#';
            if (x == s.x && y == s.y) c = 'O';
            std::cout << c;
        }
        std::cout << "|\n";
    }
    std::cout << "+------------------------+\n" << std::flush;
}
} // namespace

int main(int argc, char** argv) {
    if (argc > 3) { std::cerr << "Usage: pong-client [host] [port]\n"; return 1; }
    const char* host = argc > 1 ? argv[1] : "127.0.0.1";
    int port = 2302;
    if (argc > 2) {
        const std::string_view s(argv[2]);
        const auto parsed = std::from_chars(s.data(), s.data() + s.size(), port);
        if (parsed.ec != std::errc{} || parsed.ptr != s.data() + s.size() ||
            port < 1 || port > 65535) { std::cerr << "Invalid port\n"; return 1; }
    }
    aeon::udp_client_event_socket sock;
    if (!sock.connect(host, port)) { std::cerr << "Unable to select server\n"; return 1; }
    const aeon::address server = sock.get_remote_address();
    aeon::event_socket_set loop;
    if (!loop.add(&sock)) return 1;
    view screen{};
    int player = 0;
    bool full = false, dirty = false;
    auto last_snapshot = std::chrono::steady_clock::now();
    loop.on_socket_datagram([&](aeon::udp_event_socket*, const char* data, int n,
                                const aeon::address& from) {
        // Ignore packets from endpoints other than the selected server.
        if (from.get_port() != server.get_port() ||
            from.get_string() != server.get_string() || n <= 0) return;
        const std::string_view packet(data, static_cast<std::size_t>(n));
        if (packet == "W 1") player = 1;
        else if (packet == "W 2") player = 2;
        else if (packet == "FULL") full = true;
        else if (parse_state(packet, screen)) {
            dirty = true;
            last_snapshot = std::chrono::steady_clock::now();
        }
    });
    terminal term;
    std::signal(SIGINT, interrupt_client);
    auto last_join = std::chrono::steady_clock::now() - std::chrono::seconds(1);
    auto last_heartbeat = last_join;
    bool running = true;
    int direction = 0;
    std::cout << "Connecting to " << host << ':' << port << " ...\n";
    while (running && !full && !interruption_requested) {
        const auto now = std::chrono::steady_clock::now();
        if (player && now - last_snapshot > std::chrono::seconds(3)) {
            // Server restarted or connectivity was lost: discard old sequence
            // so a newly started authoritative server can be joined again.
            player = 0;
            direction = 0;
            screen = {};
            last_join = now - std::chrono::seconds(1);
            last_snapshot = now;
            std::cout << "\nConnection lost; rejoining...\n";
        }
        if (!player && now - last_join >= std::chrono::milliseconds(500)) {
            sock.write("J", 1);
            last_join = now;
        }
        if (player && now - last_heartbeat >= std::chrono::milliseconds(750)) {
            sock.write("H", 1);
            last_heartbeat = now;
        }
        for (int i = 0; i < 8; ++i) {
            const int k = term.key();
            if (k < 0) break;
            if (k == 'q' || k == 'Q') { running = false; break; }
            int next = direction;
            if (k == 'w' || k == 'W') next = -1;
            if (k == 's' || k == 'S') next = 1;
            if (k == 'x' || k == 'X') next = 0;
            if (next != direction && player) {
                const char c = next == -1 ? 'U' : next == 1 ? 'D' : 'N';
                sock.write(&c, 1);
                direction = next;
            }
        }
        if (!running) break;
        if (loop.poll(25) < 0) { std::cerr << "Poll error\n"; return 1; }
        if (player && dirty) { draw(screen, player); dirty = false; }
    }
    if (player) sock.write("Q", 1);
    loop.remove_udp_socket(&sock);
    if (full) std::cerr << "Server already has two players\n";
    return full ? 1 : 0;
}
