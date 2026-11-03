#pragma once
#include "socket.hpp"
#include <memory>

namespace aeon {

class LIBAEON_API server_socket : public socket, protected peer_collection {
public:
    server_socket();
    ~server_socket() override;
    server_socket(const server_socket&) = delete;
    server_socket& operator=(const server_socket&) = delete;

    int set_accept_timeout(int timeout_ms);
    int bind(const sockaddr* addr, socklen_t addrlen);
    bool listen();
    bool listen(int port);
    virtual bool listen(const char* address, int port);

    // Borrowed pointers. The listener owns every accepted peer until destruction.
    // close() closes peers, but retains their objects until destruction.
    virtual socket* accept();
    virtual socket* accept(bool blocking);
    bool close() override;
    // Explicitly retire one registered peer. External non-owning poll sets
    // must be unregistered first. Borrowed pointers become invalid.
    bool remove_peer(socket* peer);
    // The socket_set is a non-owning view; server is the actual owner.
    socket_set& peers();
    const socket_set& peers() const;
    int peer_count() const;
    socket* peer_at(unsigned int index) const;

protected:
    // Single ownership boundary for all accepted TCP peers. Implementations
    // must adopt fd immediately and close it if peer construction fails.
    virtual std::unique_ptr<socket> create_accepted_socket(socket_t fd);

private:
    int accept_timeout_ms = 0;
};
} // namespace aeon
