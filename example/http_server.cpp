#include "aeon.hpp"
#include "http_server.hpp"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <cctype>
#include <string_view>
#include <utility>

namespace aeon_examples {
namespace {
std::string lower(std::string_view input) {
    std::string result;
    result.reserve(input.size());
    for (const unsigned char c : input)
        result.push_back(static_cast<char>(std::tolower(c)));
    return result;
}

std::string_view trim(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.remove_suffix(1);
    return s;
}

bool token_name(std::string_view s) {
    if (s.empty()) return false;
    constexpr std::string_view extra = "!#$%&'*+-.^_`|~";
    for (unsigned char c : s) {
        if (!std::isalnum(c) && extra.find(static_cast<char>(c)) == std::string_view::npos)
            return false;
    }
    return true;
}

bool contains_token(std::string_view comma_separated, std::string_view match) {
    while (!comma_separated.empty()) {
        const auto next = comma_separated.find(',');
        const auto item = trim(comma_separated.substr(0, next));
        if (lower(item) == match) return true;
        if (next == std::string_view::npos) break;
        comma_separated.remove_prefix(next + 1);
    }
    return false;
}

std::string_view reason(int status) {
    switch (status) {
    case 200: return "OK";
    case 400: return "Bad Request";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 408: return "Request Timeout";
    case 413: return "Content Too Large";
    case 431: return "Request Header Fields Too Large";
    case 501: return "Not Implemented";
    case 505: return "HTTP Version Not Supported";
    default: return "Internal Server Error";
    }
}

struct parsed_request {
    int error = 0;
    bool head = false;
    bool close = true;
    bool http10 = false;
    std::string path;
};

parsed_request parse(std::string_view raw) {
    parsed_request p;
    if (raw.size() < 4 || raw.substr(raw.size() - 4) != "\r\n\r\n") {
        p.error = 400;
        return p;
    }
    // No bare controls, embedded NUL, or invalid non-ASCII bytes in this
    // deliberately ASCII-only example protocol.
    for (unsigned char c : raw) {
        if (c >= 127 || (c < 32 && c != '\r' && c != '\n' && c != '\t')) {
            p.error = 400;
            return p;
        }
    }
    const auto first_end = raw.find("\r\n");
    if (first_end == std::string_view::npos) { p.error = 400; return p; }
    const auto start = raw.substr(0, first_end);
    const auto sp1 = start.find(' ');
    const auto sp2 = sp1 == std::string_view::npos ? sp1 : start.find(' ', sp1 + 1);
    if (sp1 == std::string_view::npos || sp2 == std::string_view::npos ||
        start.find(' ', sp2 + 1) != std::string_view::npos ||
        sp1 == 0 || sp2 == sp1 + 1 || sp2 + 1 >= start.size()) {
        p.error = 400; return p;
    }
    const auto method = start.substr(0, sp1);
    const auto target = start.substr(sp1 + 1, sp2 - sp1 - 1);
    const auto version = start.substr(sp2 + 1);
    if (!token_name(method) || target.empty() || target.front() != '/' ||
        target.find('#') != std::string_view::npos ||
        target.find(' ') != std::string_view::npos) {
        p.error = 400; return p;
    }
    if (version != "HTTP/1.1" && version != "HTTP/1.0") {
        p.error = version.substr(0, 5) == "HTTP/" ? 505 : 400;
        return p;
    }
    const bool http11 = version == "HTTP/1.1";
    p.http10 = !http11;
    p.close = !http11;
    p.head = method == "HEAD";
    p.path = std::string(target.substr(0, target.find('?')));

    int hosts = 0, lengths = 0;
    bool request_body = false;
    std::string connection;
    std::size_t at = first_end + 2;
    const std::size_t end_headers = raw.size() - 2;
    while (at < end_headers) {
        const auto end = raw.find("\r\n", at);
        if (end == std::string_view::npos || end > end_headers) {
            p.error = 400; return p;
        }
        const auto line = raw.substr(at, end - at);
        at = end + 2;
        if (line.empty()) break;
        const auto colon = line.find(':');
        if (colon == std::string_view::npos || !token_name(line.substr(0, colon))) {
            p.error = 400; return p;
        }
        const auto field = lower(line.substr(0, colon));
        const auto value = trim(line.substr(colon + 1));
        if (std::any_of(value.begin(), value.end(), [](unsigned char c) {
                return c == '\r' || c == '\n' || (c < 32 && c != '\t');
            })) { p.error = 400; return p; }
        if (field == "host") {
            if (++hosts != 1 || value.empty()) { p.error = 400; return p; }
        } else if (field == "content-length") {
            if (++lengths != 1 || value.empty()) { p.error = 400; return p; }
            std::uint64_t length = 0;
            const auto parsed = std::from_chars(value.data(), value.data() + value.size(), length);
            if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size()) {
                p.error = 400; return p;
            }
            if (length != 0) request_body = true;
        } else if (field == "transfer-encoding") {
            // Never parse past an unrecognized request body as another request.
            p.error = 501; return p;
        } else if (field == "connection") {
            if (!connection.empty()) connection += ',';
            connection += value;
        }
    }
    if (http11 && hosts != 1) { p.error = 400; return p; }
    if (contains_token(connection, "close")) p.close = true;
    else if (contains_token(connection, "keep-alive")) p.close = false;
    if (request_body) { p.error = 413; return p; }
    if (method != "GET" && method != "HEAD") p.error = 405;
    return p;
}
} // namespace

http_server::http_server(http_limits limits) : limits_(limits) {
    sockets_.on_socket_listen([this](aeon::event_socket* s) {
        if (s == &listener_) accept_ready();
    });
    sockets_.on_socket_data([this](aeon::event_socket* s, const char* d, int n) {
        receive(s, d, n);
    });
    sockets_.on_socket_drain([this](aeon::event_socket* s) { drained(s); });
    sockets_.on_socket_close([this](aeon::event_socket* s) { retire(s); });
    sockets_.on_socket_error([this](aeon::event_socket* s, int) { retire(s); });
}
http_server::~http_server() { stop(); }

bool http_server::start(const char* address, int port) {
    if (running_ || polling_ || !address || limits_.max_clients == 0 ||
        limits_.max_header_bytes < 32 || limits_.max_queued_bytes < 256 ||
        limits_.idle_timeout_ms < 0) return false;
    if (!listener_.listen(address, port)) return false;
    if (!sockets_.add(&listener_)) { listener_.close(); return false; }
    running_ = true;
    return true;
}

int http_server::local_port() const {
    if (!running_ || !listener_.is_valid_socket()) return -1;
    sockaddr_storage local{};
    socklen_t size = static_cast<socklen_t>(sizeof(local));
    if (::getsockname(listener_.sockfd, reinterpret_cast<sockaddr*>(&local), &size) != 0)
        return -1;
    if (local.ss_family == AF_INET)
        return ntohs(reinterpret_cast<const sockaddr_in*>(&local)->sin_port);
    if (local.ss_family == AF_INET6)
        return ntohs(reinterpret_cast<const sockaddr_in6*>(&local)->sin6_port);
    return -1;
}

std::size_t http_server::client_count() const {
    return static_cast<std::size_t>(std::count_if(peers_.begin(), peers_.end(),
        [](const auto& p) { return !p->retire; }));
}

http_server::peer* http_server::lookup(aeon::event_socket* s) {
    const auto it = std::find_if(peers_.begin(), peers_.end(),
        [s](const auto& p) { return p->socket == s; });
    return it == peers_.end() ? nullptr : it->get();
}

void http_server::accept_ready() {
    for (int i = 0; i < 32; ++i) {
        auto accepted = listener_.accept(false);
        if (!accepted) break;
        if (client_count() >= limits_.max_clients ||
            !accepted->set_max_queued_bytes(limits_.max_queued_bytes)) {
            accepted->close();
            continue;
        }
        auto p = std::make_unique<peer>();
        p->socket = accepted;
        p->last_activity = clock::now();
        auto* socket = p->socket;
        if (sockets_.add(socket)) peers_.push_back(std::move(p));
    }
}

void http_server::retire(aeon::event_socket* s) {
    auto* p = lookup(s);
    if (!p || p->retire) return;
    p->retire = true;
    sockets_.remove_socket(s);
}

void http_server::drained(aeon::event_socket* s) {
    auto* p = lookup(s);
    if (p && p->closing) retire(s);
}

bool http_server::send_response(peer& p, int status, const std::string& type,
                                const std::string& body, bool head, bool close,
                                bool allow_header, bool http10) {
    std::string response = std::string(http10 ? "HTTP/1.0 " : "HTTP/1.1 ") + std::to_string(status) + " " + std::string(reason(status)) + "\r\n";
    response += "Content-Type: " + type + "\r\n";
    response += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    response += "Connection: ";
    response += close ? "close\r\n" : "keep-alive\r\n";
    if (allow_header) response += "Allow: GET, HEAD\r\n";
    response += "\r\n";
    if (!head) response += body;
    if (!p.socket->queue_write(response)) {
        retire(p.socket);
        return false;
    }
    if (close) p.closing = true;
    return true;
}

void http_server::handle_request(peer& p, const std::string& raw) {
    const auto request = parse(raw);
    if (request.error) {
        const std::string body = std::string(reason(request.error)) + "\n";
        send_response(p, request.error, "text/plain; charset=utf-8", body,
                      false, true, request.error == 405, request.http10);
        return;
    }
    if (request.path == "/" || request.path == "/index.html") {
        const std::string body = "<html><body><h1>libAeon HTTP example</h1></body></html>\n";
        send_response(p, 200, "text/html; charset=utf-8", body, request.head, request.close, false, request.http10);
    } else if (request.path == "/health") {
        send_response(p, 200, "text/plain; charset=utf-8", "ok\n", request.head, request.close, false, request.http10);
    } else {
        send_response(p, 404, "text/plain; charset=utf-8", "Not Found\n", request.head, request.close, false, request.http10);
    }
}

void http_server::receive(aeon::event_socket* s, const char* data, int size) {
    auto* p = lookup(s);
    if (!p || p->retire || p->closing || !data || size <= 0) return;
    p->last_activity = clock::now();
    // Bound aggregate incoming unparsed bytes and never accept an oversized
    // request by waiting for another socket read.
    p->incoming.append(data, static_cast<std::size_t>(size));
    constexpr int max_pipelined_per_event = 32;
    for (int processed = 0; !p->retire && !p->closing; ++processed) {
        const auto end = p->incoming.find("\r\n\r\n");
        if (end == std::string::npos) {
            if (p->incoming.size() > limits_.max_header_bytes) {
                send_response(*p, 431, "text/plain; charset=utf-8",
                              "Request Header Fields Too Large\n", false, true);
            }
            break;
        }
        if (end + 4 > limits_.max_header_bytes) {
            send_response(*p, 431, "text/plain; charset=utf-8",
                          "Request Header Fields Too Large\n", false, true);
            break;
        }
        if (processed >= max_pipelined_per_event) {
            send_response(*p, 400, "text/plain; charset=utf-8",
                          "Too many pipelined requests\n", false, true);
            break;
        }
        std::string request = p->incoming.substr(0, end + 4);
        p->incoming.erase(0, end + 4);
        handle_request(*p, request);
    }
    if (p->closing) p->incoming.clear();
    // Pipelined traffic is limited to one bounded read's worth of parsing.
    if (!p->retire && !p->closing && p->incoming.size() > limits_.max_header_bytes)
        send_response(*p, 431, "text/plain; charset=utf-8",
                      "Request Header Fields Too Large\n", false, true);
}

void http_server::reap() {
    for (auto it = peers_.begin(); it != peers_.end();) {
        if (!(*it)->retire) { ++it; continue; }
        // Socket has already been deregistered by retire().
        listener_.remove_peer((*it)->socket);
        it = peers_.erase(it);
    }
}

int http_server::poll(int timeout_ms) {
    if (!running_ || polling_) return -1;
    polling_ = true;
    int result = -1;
    try { result = sockets_.poll(timeout_ms); }
    catch (...) { polling_ = false; reap(); throw; }
    polling_ = false;
    if (limits_.idle_timeout_ms > 0) {
        const auto now = clock::now();
        for (const auto& p : peers_) {
            if (!p->retire && now - p->last_activity >
                std::chrono::milliseconds(limits_.idle_timeout_ms))
                retire(p->socket);
        }
    }
    reap();
    return result;
}

void http_server::stop() {
    if (polling_) return;
    sockets_.remove_socket(&listener_);
    for (auto& p : peers_) sockets_.remove_socket(p->socket);
    peers_.clear();
    listener_.close();
    running_ = false;
}

} // namespace aeon_examples
