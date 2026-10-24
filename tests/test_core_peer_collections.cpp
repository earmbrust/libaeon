#include "aeon.hpp"
#include <gtest/gtest.h>
#include <cstring>

namespace {
int bound_port(const aeon::socket& owner) {
    sockaddr_storage endpoint{};
    socklen_t length = sizeof(endpoint);
    if (::getsockname(owner.sockfd, reinterpret_cast<sockaddr*>(&endpoint), &length)) return -1;
    if (endpoint.ss_family == AF_INET)
        return ntohs(reinterpret_cast<const sockaddr_in*>(&endpoint)->sin_port);
    if (endpoint.ss_family == AF_INET6)
        return ntohs(reinterpret_cast<const sockaddr_in6*>(&endpoint)->sin6_port);
    return -1;
}
}

TEST(CorePeerCollection, TcpOwnershipAndRemoval) {
    aeon::server_socket server;
    ASSERT_TRUE(server.listen("127.0.0.1", 0));
    const int port = bound_port(server);
    ASSERT_GT(port, 0);
    aeon::client_socket client;
    ASSERT_TRUE(client.connect("127.0.0.1", port));
    auto* accepted = server.accept(true);
    ASSERT_NE(accepted, nullptr);
    EXPECT_EQ(server.peers().size(), 1);
    EXPECT_EQ(server.peer_count(), 1);
    EXPECT_EQ(server.peer_at(0), accepted);
    EXPECT_EQ(accepted->parent_server(), &server);
    EXPECT_TRUE(server.remove_peer(accepted));
    EXPECT_EQ(server.peer_count(), 0);
    EXPECT_EQ(server.peers().size(), 0);
}

TEST(CorePeerCollection, UdpOwnershipAndRemoval) {
    aeon::udp_server_socket server;
    ASSERT_TRUE(server.listen(0));
    const int port = bound_port(server);
    ASSERT_GT(port, 0);
    aeon::udp_client_socket client;
    ASSERT_TRUE(client.connect("127.0.0.1", port));
    ASSERT_EQ(client.write("abc", 3), 3);
    char buffer[8]{};
    ASSERT_EQ(server.read(buffer, sizeof(buffer)), 3);
    EXPECT_EQ(std::memcmp(buffer, "abc", 3), 0);
    ASSERT_EQ(server.peers().size(), 1);
    auto* peer = server.get_peer(server.get_remote_address());
    ASSERT_NE(peer, nullptr);
    EXPECT_EQ(peer, server.get_peer(server.get_remote_address()));
    EXPECT_EQ(peer->parent_socket(), &server);
    EXPECT_TRUE(server.remove_peer(peer));
    EXPECT_EQ(server.peers().size(), 0);
}
