#pragma once
#include "udp_socket.hpp"
#include "internal/peer_collection.hpp"

namespace aeon {
class LIBAEON_API udp_server_socket : public udp_socket, protected internal::peer_collection {
public:
    udp_server_socket();
    ~udp_server_socket() override;
    bool listen(int port);
    bool listen();
    bool close() override;
    udp_socket* get_peer(const address& remote);
    bool remove_peer(udp_socket* peer);
    socket_set& peers();
    const socket_set& peers() const;
    // Listening read records a logical peer for the last source address.
    int read(char* buffer, int size) override;
    using udp_socket::read;
private:
};
} // namespace aeon
