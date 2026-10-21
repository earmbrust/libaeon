#include <gtest/gtest.h>
#include "aeon.hpp"
#include <cstring>
#include <memory>
#include <string>
#include <type_traits>

namespace {
int bound_port(const aeon::socket& s) {
    sockaddr_storage local{};
    socklen_t len = static_cast<socklen_t>(sizeof(local));
    if (::getsockname(s.sockfd, reinterpret_cast<sockaddr*>(&local), &len) != 0) return -1;
    if (local.ss_family == AF_INET6)
        return ntohs(reinterpret_cast<const sockaddr_in6*>(&local)->sin6_port);
    if (local.ss_family == AF_INET)
        return ntohs(reinterpret_cast<const sockaddr_in*>(&local)->sin_port);
    return -1;
}
bool descriptor_open(aeon::socket_t fd) {
    int type = 0;
#ifdef NET_PLATFORM_WINDOWS
    int len = sizeof(type);
#else
    socklen_t len = sizeof(type);
#endif
    return ::getsockopt(fd, SOL_SOCKET, SO_TYPE, reinterpret_cast<char*>(&type), &len) == 0;
}
} // namespace

TEST(ServerPeerLifecycle, TcpListenerOwnsAndAssociatesAcceptedPeer) {
    static_assert(std::is_same_v<decltype(std::declval<aeon::server_socket&>().accept()), aeon::socket*>);
    auto server = std::make_unique<aeon::server_socket>();
    ASSERT_TRUE(server->listen("127.0.0.1", 0));
    aeon::client_socket client;
    ASSERT_TRUE(client.connect("127.0.0.1", bound_port(*server)));
    auto* peer = server->accept(true);
    ASSERT_NE(peer, nullptr);
    EXPECT_EQ(peer->parent_server(), server.get());
    ASSERT_EQ(server->peer_count(), 1);
    EXPECT_EQ(server->peers().at(0), peer);
    auto fd = peer->sockfd;
    ASSERT_TRUE(server->close());
    EXPECT_FALSE(peer->is_valid_socket());
    EXPECT_FALSE(descriptor_open(fd));
    // close() invalidates but does not destroy peer object.
    EXPECT_EQ(peer->parent_server(), server.get());
    server.reset(); // peer destroyed with server: no further use of peer.
}

TEST(ServerPeerLifecycle, TcpEventListenerOwnsPeersAndSupportsRetirement) {
    static_assert(std::is_same_v<decltype(std::declval<aeon::server_event_socket&>().accept()), aeon::event_socket*>);
    aeon::server_event_socket server;
    ASSERT_TRUE(server.listen("127.0.0.1", 0));
    aeon::client_socket client;
    ASSERT_TRUE(client.connect("127.0.0.1", bound_port(server)));
    auto* peer = server.accept(false);
    ASSERT_NE(peer, nullptr);
    EXPECT_EQ(peer->parent_server(), &server);
    EXPECT_EQ(server.peers().size(), 1);
    aeon::event_socket_set poller;
    ASSERT_TRUE(poller.add(peer));
    ASSERT_TRUE(poller.remove_socket(peer)); // deregister before destruction
    EXPECT_TRUE(server.remove_peer(peer));
    EXPECT_EQ(server.peer_count(), 0);
    EXPECT_EQ(server.peers().size(), 0);
}

TEST(ServerPeerLifecycle, UdpPeersShareOnlyTheParentTransport) {
    aeon::udp_server_socket server;
    ASSERT_TRUE(server.listen(0));
    const auto native = server.sockfd;
    aeon::udp_client_socket a, b;
    ASSERT_TRUE(a.connect("127.0.0.1", bound_port(server)));
    ASSERT_TRUE(b.connect("127.0.0.1", bound_port(server)));
    ASSERT_EQ(a.set_read_timeout(500), 0);
    ASSERT_EQ(b.set_read_timeout(500), 0);
    char buffer[16]{};
    ASSERT_EQ(a.write("A", 1), 1);
    ASSERT_EQ(server.read(buffer, sizeof(buffer)), 1);
    auto* pa = server.get_peer(server.get_remote_address());
    ASSERT_NE(pa, nullptr);
    ASSERT_EQ(b.write("B", 1), 1);
    ASSERT_EQ(server.read(buffer, sizeof(buffer)), 1);
    auto* pb = server.get_peer(server.get_remote_address());
    ASSERT_NE(pb, nullptr);
    ASSERT_NE(pa, pb);
    EXPECT_EQ(pa->parent_server(), &server);
    EXPECT_EQ(pb->parent_server(), &server);
    EXPECT_EQ(pa->sockfd, aeon::invalid_socket);
    EXPECT_EQ(pb->sockfd, aeon::invalid_socket);
    EXPECT_TRUE(descriptor_open(native));
    EXPECT_EQ(server.peers().size(), 2);
    EXPECT_EQ(server.peers().broadcast("hi"), 2);
    EXPECT_EQ(a.read(buffer, sizeof(buffer)), 2);
    EXPECT_EQ(std::string(buffer, 2), "hi");
    EXPECT_EQ(b.read(buffer, sizeof(buffer)), 2);
    EXPECT_EQ(std::string(buffer, 2), "hi");
    int visited = 0;
    for (auto* p : server.peers()) { EXPECT_NE(p, nullptr); ++visited; }
    EXPECT_EQ(visited, 2);
    ASSERT_TRUE(server.close());
    EXPECT_FALSE(descriptor_open(native));
    EXPECT_FALSE(pa->is_valid_socket());
    EXPECT_FALSE(pb->is_valid_socket());
    EXPECT_EQ(pa->write("no", 2), NET_SOCKET_ERROR);
    EXPECT_EQ(server.peers().size(), 2); // closed but retained objects
}

TEST(ServerPeerLifecycle, UdpEventRegistersSenderBeforeCallback) {
    aeon::udp_server_event_socket server;
    ASSERT_TRUE(server.listen(0));
    aeon::udp_client_socket client;
    ASSERT_TRUE(client.connect("127.0.0.1", bound_port(server)));
    aeon::event_socket_set events;
    ASSERT_TRUE(events.add(&server));
    int callbacks = 0;
    events.on_socket_datagram([&](aeon::udp_event_socket* source, const char*, int n,
                                  const aeon::address& remote) {
        EXPECT_EQ(source, &server);
        EXPECT_EQ(n, 1);
        EXPECT_EQ(server.peers().size(), 1);
        auto* peer = server.get_peer(remote);
        ASSERT_NE(peer, nullptr);
        EXPECT_EQ(peer->parent_server(), &server);
        EXPECT_EQ(peer->write("reply", 5), 5);
        ++callbacks;
    });
    ASSERT_EQ(client.write("X", 1), 1);
    ASSERT_GT(events.poll(2000), 0);
    EXPECT_EQ(callbacks, 1);
    ASSERT_TRUE(events.remove_udp_socket(&server));
    EXPECT_TRUE(server.close());
    EXPECT_EQ(server.peers().size(), 1);
}
