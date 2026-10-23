#pragma once
#include "socket.hpp"
#include "socket_set.hpp"

namespace aeon {
namespace internal { struct tcp_peer_store; }

class LIBAEON_API server_socket : public socket {
public:
    server_socket();
    ~server_socket() override;
    server_socket(const server_socket&) = delete;
    server_socket& operator=(const server_socket&) = delete;

    int set_accept_timeout(int timeout_ms);
    int bind(const sockaddr* addr, socklen_t addrlen);
    bool listen();
    bool listen(int port);
    bool listen(const char* address, int port);

    // Borrowed pointers. The listener owns every accepted peer until destruction.
    // close() closes peers, but retains their objects until destruction.
    socket* accept();
    socket* accept(bool blocking);
    bool close() override;
    // Explicitly retire one registered peer. External non-owning poll sets
    // must be unregistered first. Borrowed pointers become invalid.
    bool remove_peer(socket* peer);
    // The socket_set is a non-owning view; server is the actual owner.
    socket_set& peers();
    const socket_set& peers() const;
    int peer_count() const;
    socket* peer_at(unsigned int index) const;

private:
    int accept_timeout_ms = 0;
    internal::tcp_peer_store* peers_;
};
} // namespace aeon
