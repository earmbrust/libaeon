#include "chat_server.hpp"

#include <algorithm>
#include <string>
#include <utility>

namespace aeon_examples {

chat_server::chat_server(chat_limits limits) : limits_(limits) {
    sockets_.on_socket_listen([this](aeon::event_socket* socket) {
        if (socket == &listener_) accept_ready();
    });
    sockets_.on_socket_data([this](aeon::event_socket* socket, const char* bytes, int count) {
        receive(socket, bytes, count);
    });
    sockets_.on_socket_close([this](aeon::event_socket* socket) { retire(socket); });
    sockets_.on_socket_error([this](aeon::event_socket* socket, int) { retire(socket); });
}

chat_server::~chat_server() { stop(); }

bool chat_server::start(const char* bind_address, int port) {
    if (running_ || polling_ || !bind_address || limits_.max_clients == 0 ||
        limits_.max_line_bytes == 0 || limits_.max_queued_bytes == 0) return false;
    if (!listener_.listen(bind_address, port)) return false;
    if (!sockets_.add(&listener_)) {
        listener_.close();
        return false;
    }
    running_ = true;
    return true;
}

int chat_server::local_port() const {
    if (!running_ || !listener_.is_valid_socket()) return -1;
    sockaddr_storage local{};
    socklen_t length = static_cast<socklen_t>(sizeof(local));
    if (::getsockname(listener_.sockfd, reinterpret_cast<sockaddr*>(&local), &length) != 0)
        return -1;
    if (local.ss_family == AF_INET)
        return ntohs(reinterpret_cast<const sockaddr_in*>(&local)->sin_port);
    if (local.ss_family == AF_INET6)
        return ntohs(reinterpret_cast<const sockaddr_in6*>(&local)->sin6_port);
    return -1;
}

std::size_t chat_server::client_count() const {
    return static_cast<std::size_t>(std::count_if(peers_.begin(), peers_.end(),
        [](const auto& p) { return !p->retire; }));
}

chat_server::peer* chat_server::lookup(aeon::event_socket* socket) {
    const auto it = std::find_if(peers_.begin(), peers_.end(),
        [socket](const auto& p) { return p->socket == socket; });
    return it == peers_.end() ? nullptr : it->get();
}

void chat_server::accept_ready() {
    // Bound the work for this readiness event to preserve fairness.
    for (int i = 0; i < 32; ++i) {
        auto accepted = listener_.accept(false);
        if (!accepted) break;
        if (client_count() >= limits_.max_clients) {
            accepted->close();
            continue;
        }
        if (!accepted->set_max_queued_bytes(limits_.max_queued_bytes)) {
            accepted->close();
            continue;
        }
        auto next = std::make_unique<peer>();
        next->socket = accepted;
        next->id = next_id_++;
        auto* native = next->socket;
        if (!sockets_.add(native)) continue;
        peers_.push_back(std::move(next));
        const std::string greeting = "* connected as guest-" +
            std::to_string(peers_.back()->id) + "\n";
        if (!native->queue_write(greeting)) retire(native);
    }
}

void chat_server::retire(aeon::event_socket* socket) {
    auto* p = lookup(socket);
    if (!p || p->retire) return;
    p->retire = true;
    // Only unregister here. Destruction and close happen after poll() returns.
    sockets_.remove_socket(socket);
}

void chat_server::broadcast(const peer& sender, const std::string& line) {
    const std::string frame = "guest-" + std::to_string(sender.id) + ": " + line + "\n";
    for (const auto& p : peers_) {
        if (!p->retire && !p->socket->queue_write(frame)) retire(p->socket);
    }
}

void chat_server::receive(aeon::event_socket* socket, const char* bytes, int count) {
    auto* sender = lookup(socket);
    if (!sender || sender->retire || !bytes || count <= 0) return;
    for (int i = 0; i < count && !sender->retire; ++i) {
        const char c = bytes[i];
        if (c == '\0') {
            retire(socket); // Plain-text protocol; reject embedded NULs.
            break;
        }
        if (c == '\n') {
            if (!sender->pending_line.empty() && sender->pending_line.back() == '\r')
                sender->pending_line.pop_back();
            if (!sender->pending_line.empty()) broadcast(*sender, sender->pending_line);
            sender->pending_line.clear();
        } else if (sender->pending_line.size() >= limits_.max_line_bytes) {
            retire(socket); // Do not grow unbounded buffers on unterminated input.
        } else {
            sender->pending_line.push_back(c);
        }
    }
}

void chat_server::reap() {
    for (auto it = peers_.begin(); it != peers_.end();) {
        if (!(*it)->retire) { ++it; continue; }
        // Socket has already been deregistered by retire().
        listener_.remove_peer((*it)->socket);
        it = peers_.erase(it);
    }
}

int chat_server::poll(int timeout_ms) {
    if (!running_ || polling_) return -1;
    polling_ = true;
    int handled = -1;
    try {
        handled = sockets_.poll(timeout_ms);
    } catch (...) {
        polling_ = false;
        reap();
        throw;
    }
    polling_ = false;
    reap();
    return handled;
}

void chat_server::stop() {
    if (polling_) return; // Contract: stop() is only called outside poll().
    sockets_.remove_socket(&listener_);
    for (auto& p : peers_) sockets_.remove_socket(p->socket);
    peers_.clear();
    listener_.close();
    running_ = false;
}

} // namespace aeon_examples
