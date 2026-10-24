#pragma once

#include "socket_set.hpp"
#include <memory>

namespace aeon {
class udp_socket;
namespace internal {

class peer_collection;

// Owns peers and maintains the non-owning socket_set view used by servers.
class LIBAEON_API peer_collection {
protected:
    peer_collection();
    ~peer_collection();
    peer_collection(const peer_collection&) = delete;
    peer_collection& operator=(const peer_collection&) = delete;

    socket* add_peer(std::unique_ptr<socket> peer);
    bool remove_owned_peer(socket* peer);
    bool close_owned_peers();
    socket_set& peer_members();
    const socket_set& peer_members() const;
    int owned_peer_count() const;
    socket* owned_peer_at(unsigned int index) const;
    socket* find_udp_peer(const address& remote) const;
    udp_socket* get_or_create_udp_peer(udp_socket* server, const address& remote);

private:
    struct state;
    state* peers_;
};

} // namespace internal
} // namespace aeon
