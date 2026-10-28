#pragma once
#include "udp_event_socket.hpp"

namespace aeon {
class LIBAEON_API udp_server_event_socket : public udp_event_socket, protected internal::peer_collection {
public:
    udp_server_event_socket();
    ~udp_server_event_socket() override;
    bool listen(int port);
    bool listen();
    bool close() override;
    int read(char* buffer, int size) override;
    /// \cond
    using udp_socket::read;
    /// \endcond
    udp_socket* get_peer(const address& remote);
    bool remove_peer(udp_socket* peer);
    socket_set& peers();
    const socket_set& peers() const;
protected:
    void register_sender(const address& remote) override;
private:
};
} // namespace aeon
