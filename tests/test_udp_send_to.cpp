#include "aeon.hpp"
#include <gtest/gtest.h>
#include <string>

TEST(UdpSendTo, TargetedRepliesReachTwoDifferentClients) {
    aeon::udp_server_event_socket server;
    ASSERT_TRUE(server.listen(0));
    sockaddr_storage local{};
    socklen_t len = static_cast<socklen_t>(sizeof(local));
    ASSERT_EQ(::getsockname(server.sockfd, reinterpret_cast<sockaddr*>(&local), &len), 0);
    const int port = local.ss_family == AF_INET6
        ? ntohs(reinterpret_cast<sockaddr_in6*>(&local)->sin6_port)
        : ntohs(reinterpret_cast<sockaddr_in*>(&local)->sin_port);
    aeon::udp_client_event_socket a, b;
    ASSERT_TRUE(a.connect("127.0.0.1", port));
    ASSERT_TRUE(b.connect("127.0.0.1", port));
    aeon::event_socket_set network;
    ASSERT_TRUE(network.add(&server));
    ASSERT_TRUE(network.add(&a));
    ASSERT_TRUE(network.add(&b));
    int source_a_port = 0, source_b_port = 0;
    std::string received_a, received_b;
    network.on_socket_datagram([&](aeon::udp_event_socket* socket, const char* data,
                                   int n, const aeon::address& from) {
        if (socket == &server) {
            // Use the real source endpoints seen by the listener. A UDP
            // client's getsockname() may report 0.0.0.0 for an implicit bind,
            // which is not a valid unicast reply destination on Windows.
            if (n == 1 && data[0] == 'A') source_a_port = from.get_port();
            if (n == 1 && data[0] == 'B') source_b_port = from.get_port();
        } else if (socket == &a) received_a.assign(data, static_cast<std::size_t>(n));
        else if (socket == &b) received_b.assign(data, static_cast<std::size_t>(n));
    });
    ASSERT_EQ(a.write("A", 1), 1);
    ASSERT_EQ(b.write("B", 1), 1);
    for (int i = 0; i < 20 && (!source_a_port || !source_b_port); ++i)
        ASSERT_GE(network.poll(5), 0);
    ASSERT_GT(source_a_port, 0);
    ASSERT_GT(source_b_port, 0);
    ASSERT_TRUE(source_a_port != source_b_port);
    // Supply explicit, routable IPv4 loopback addresses so peer writes must
    // map them to IPv6 destinations on dual-stack listeners. The ports are
    // taken from received datagrams, never from an unspecified local bind.
    sockaddr_in source_a{};
    source_a.sin_family = AF_INET;
    source_a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    source_a.sin_port = htons(static_cast<unsigned short>(source_a_port));
    sockaddr_in source_b{};
    source_b.sin_family = AF_INET;
    source_b.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    source_b.sin_port = htons(static_cast<unsigned short>(source_b_port));
    const auto prior = server.get_remote_address().get_port();
    EXPECT_EQ(server.get_peer(aeon::address(source_a))->write(std::string("first\0binary", 12)), 12);
    EXPECT_EQ(server.get_peer(aeon::address(source_b))->write("second", 6), 6);
    EXPECT_EQ(server.peers().size(), 2); // IPv4 and IPv4-mapped addresses deduplicate
    EXPECT_EQ(server.get_remote_address().get_port(), prior);
    for (int i = 0; i < 20 && (received_a.empty() || received_b.empty()); ++i)
        ASSERT_GE(network.poll(5), 0);
    EXPECT_EQ(received_a, std::string("first\0binary", 12));
    EXPECT_EQ(received_b, "second");
}
TEST(UdpSendTo, RejectsInvalidBufferSizes) {
    aeon::udp_server_event_socket server;
    ASSERT_TRUE(server.listen(0));
    sockaddr_in target{};
    target.sin_family = AF_INET;
    target.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    target.sin_port = htons(40000);
    auto* peer = server.get_peer(aeon::address(target));
    ASSERT_NE(peer, nullptr);
    EXPECT_EQ(peer->write("x", -1), NET_SOCKET_ERROR);
    EXPECT_EQ(peer->write(static_cast<const char*>(nullptr), 1), NET_SOCKET_ERROR);
    EXPECT_EQ(peer->parent_server(), &server);
}

TEST(UdpSendTo, ZeroLengthDatagramIsValid) {
    aeon::udp_server_event_socket server;
    ASSERT_TRUE(server.listen(0));
    sockaddr_storage local{};
    socklen_t len = static_cast<socklen_t>(sizeof(local));
    ASSERT_EQ(::getsockname(server.sockfd, reinterpret_cast<sockaddr*>(&local), &len), 0);
    const int port = local.ss_family == AF_INET6
        ? ntohs(reinterpret_cast<sockaddr_in6*>(&local)->sin6_port)
        : ntohs(reinterpret_cast<sockaddr_in*>(&local)->sin_port);
    aeon::udp_client_event_socket client;
    ASSERT_TRUE(client.connect("127.0.0.1", port));
    ASSERT_EQ(client.write("X", 1), 1);
    aeon::event_socket_set events;
    ASSERT_TRUE(events.add(&server));
    aeon::address sender;
    events.on_socket_datagram([&](aeon::udp_event_socket*, const char*, int,
                                   const aeon::address& from) { sender = from; });
    for (int i = 0; i < 12 && sender.get_port() == 0; ++i) ASSERT_GE(events.poll(5), 0);
    ASSERT_GT(sender.get_port(), 0);
    EXPECT_EQ(server.get_peer(sender)->write(static_cast<const char*>(nullptr), 0), 0);
    EXPECT_EQ(server.get_peer(sender)->write(std::string{}), 0);
    aeon::event_socket_set receiver;
    ASSERT_TRUE(receiver.add(&client));
    int zero_count = 0;
    receiver.on_socket_datagram([&](aeon::udp_event_socket*, const char*, int n,
                                    const aeon::address&) { if (n == 0) ++zero_count; });
    for (int i = 0; i < 12 && zero_count < 2; ++i) ASSERT_GE(receiver.poll(5), 0);
    EXPECT_EQ(zero_count, 2);
}
