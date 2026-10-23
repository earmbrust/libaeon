#pragma once
#include "udp_event_socket.hpp"
#include "socket_set.hpp"

namespace aeon {
namespace internal { struct udp_peer_store; }
class LIBAEON_API udp_server_event_socket : public udp_event_socket {
public:
    udp_server_event_socket();
    ~udp_server_event_socket() override;
    bool listen(int port);
    bool listen();
    bool close() override;
    int read(char* buffer, int size) override;
    int read() override;
    std::string read(int size) override;
    udp_socket* get_peer(const address& remote);
    bool remove_peer(udp_socket* peer);
    socket_set& peers();
    const socket_set& peers() const;
protected:
    void register_sender(const address& remote) override;
private:
    internal::udp_peer_store* peers_;
};
} // namespace aeon
