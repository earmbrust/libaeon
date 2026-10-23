#pragma once
#include "event_socket.hpp"
#include "socket_set.hpp"

namespace aeon {
namespace internal { struct event_peer_store; }

class LIBAEON_API server_event_socket : public event_socket {
public:
    server_event_socket();
    explicit server_event_socket(int port);
    ~server_event_socket() override;

    int set_accept_timeout(int timeout_ms);
    int bind(const sockaddr* addr, socklen_t len);
    bool listen();
    bool listen(int port);
    bool listen(const char* address, int port);
    // Borrowed, server-owned event peers. Unregister from external poll sets
    // before listener destruction (event_socket_set registrations are non-owning).
    event_socket* accept();
    event_socket* accept(bool blocking);
    bool close() override;
    // Explicitly retire one registered peer. External non-owning poll sets
    // must be unregistered first. Borrowed pointers become invalid.
    bool remove_peer(event_socket* peer);
    // The socket_set is a non-owning view; server is the actual owner.
    socket_set& peers();
    const socket_set& peers() const;
    int peer_count() const;
    event_socket* peer_at(unsigned int index) const;

protected:
    void on_data_impl(const char* buffer, int size) override;
    void on_listen_impl() override;
    void on_close_impl() override;
    void on_error_impl(int error_code) override;

private:
    int accept_timeout_ms_ = 0;
    internal::event_peer_store* peers_;
};
} // namespace aeon
