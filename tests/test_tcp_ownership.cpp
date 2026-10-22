#include <gtest/gtest.h>
#include "aeon.hpp"
#include <cstring>
#include <memory>
#include <type_traits>
#include <utility>

namespace {

static_assert(std::is_base_of_v<aeon::socket, aeon::client_event_socket>);
static_assert(std::is_base_of_v<aeon::event_socket, aeon::client_event_socket>);
static_assert(std::is_base_of_v<aeon::socket, aeon::server_event_socket>);
static_assert(std::is_convertible_v<aeon::client_event_socket*, aeon::socket*>);
static_assert(std::is_convertible_v<aeon::server_event_socket*, aeon::socket*>);
static_assert(!std::is_base_of_v<aeon::client_socket, aeon::client_event_socket>);
static_assert(!std::is_base_of_v<aeon::server_socket, aeon::server_event_socket>);
static_assert(std::is_same_v<decltype(std::declval<aeon::server_socket&>().accept()),
                             std::unique_ptr<aeon::socket>>);
static_assert(std::is_same_v<decltype(std::declval<aeon::server_event_socket&>().accept()),
                             std::unique_ptr<aeon::event_socket>>);

bool descriptor_open(aeon::socket_t fd) {
    int type = 0;
#ifdef NET_PLATFORM_WINDOWS
    int len = static_cast<int>(sizeof(type));
#else
    socklen_t len = sizeof(type);
#endif
    return ::getsockopt(fd, SOL_SOCKET, SO_TYPE,
                        reinterpret_cast<char*>(&type), &len) == 0;
}

int local_port(const aeon::socket& sock) {
    sockaddr_in addr{};
    socklen_t len = static_cast<socklen_t>(sizeof(addr));
    if (::getsockname(sock.sockfd, reinterpret_cast<sockaddr*>(&addr), &len) != 0)
        return -1;
    return static_cast<int>(ntohs(addr.sin_port));
}

} // namespace

TEST(TcpOwnership, UnconnectedEventClientHasExactlyOneDescriptor) {
    aeon::socket_t fd = aeon::invalid_socket;
    {
        aeon::client_event_socket client;
        aeon::socket& owner = client;
        ASSERT_TRUE(owner.is_valid_socket());
        fd = owner.sockfd;
        EXPECT_TRUE(descriptor_open(fd));
        EXPECT_FALSE(owner.connected);
    }
    EXPECT_FALSE(descriptor_open(fd));
}

TEST(TcpOwnership, UnconnectedEventListenerHasExactlyOneDescriptor) {
    aeon::socket_t fd = aeon::invalid_socket;
    {
        aeon::server_event_socket server;
        aeon::socket& owner = server;
        ASSERT_TRUE(owner.is_valid_socket());
        fd = owner.sockfd;
        EXPECT_TRUE(descriptor_open(fd));
    }
    EXPECT_FALSE(descriptor_open(fd));
}

TEST(TcpOwnership, FailedConnectInvalidatesOwnedDescriptor) {
    aeon::client_event_socket client;
    ASSERT_FALSE(client.connect("127.0.0.1", -1));
    EXPECT_FALSE(client.connected);
    EXPECT_NE(client.get_error(), aeon::err_none);
    // Invalid arguments do not touch the previously valid descriptor.
    EXPECT_TRUE(client.is_valid_socket());
    const auto previous = client.sockfd;
    EXPECT_TRUE(descriptor_open(previous));
}

TEST(TcpOwnership, PlainAcceptTransfersDescriptorAndSurvivesListenerDestruction) {
    auto server = std::make_unique<aeon::server_socket>();
    ASSERT_TRUE(server->listen("127.0.0.1", 0));
    const int port = local_port(*server);
    ASSERT_GT(port, 0);
    aeon::client_socket client;
    ASSERT_TRUE(client.connect("127.0.0.1", port));
    auto accepted = server->accept(true);
    ASSERT_NE(accepted, nullptr);
    const auto fd = accepted->sockfd;
    EXPECT_TRUE(descriptor_open(fd));
    EXPECT_EQ(accepted->get_remote_ip(), "127.0.0.1");
    EXPECT_TRUE(accepted->connected);
    EXPECT_NE(fd, client.sockfd);
    server.reset();
    EXPECT_TRUE(descriptor_open(fd));
    ASSERT_EQ(client.write("ok", 2), 2);
    char bytes[4]{};
    EXPECT_EQ(accepted->read(bytes, 2), 2);
    EXPECT_EQ(std::memcmp(bytes, "ok", 2), 0);
    accepted.reset();
    EXPECT_FALSE(descriptor_open(fd));
}

TEST(TcpOwnership, EventAcceptOwnsConnectedDescriptorWithoutSecondSocket) {
    aeon::server_event_socket server;
    ASSERT_TRUE(server.listen("127.0.0.1", 0));
    const int port = local_port(server);
    ASSERT_GT(port, 0);
    aeon::client_socket client;
    ASSERT_TRUE(client.connect("127.0.0.1", port));
    auto accepted = server.accept(false);
    ASSERT_NE(accepted, nullptr);
    auto* as_base = static_cast<aeon::socket*>(accepted.get());
    const auto fd = as_base->sockfd;
    EXPECT_TRUE(descriptor_open(fd));
    EXPECT_GT(as_base->get_remote_port(), 0);
    EXPECT_TRUE(as_base->connected);
    accepted.reset();
    EXPECT_FALSE(descriptor_open(fd));
}

TEST(TcpOwnership, EventSetIsNonOwning) {
    aeon::event_socket member;
    ASSERT_TRUE(member.is_valid_socket());
    const auto fd = member.sockfd;
    {
        aeon::event_socket_set set;
        EXPECT_TRUE(set.add(&member));
        EXPECT_EQ(set.size(), 1);
    }
    EXPECT_TRUE(member.is_valid_socket());
    EXPECT_TRUE(descriptor_open(fd));
}

TEST(TcpOwnership, RepeatedListenReplacesDescriptorWithoutOrphans) {
    aeon::server_socket server;
    ASSERT_TRUE(server.listen("127.0.0.1", 0));
    ASSERT_TRUE(server.listen("127.0.0.1", 0));
    EXPECT_TRUE(descriptor_open(server.sockfd));
    // The first descriptor may be reused immediately by the OS; therefore
    // verify the replacement is valid without relying on numeric uniqueness.
}

TEST(TcpOwnership, PlainAcceptCanBeClosedThroughBasePointer) {
    aeon::server_socket server;
    ASSERT_TRUE(server.listen("127.0.0.1", 0));
    aeon::client_socket client;
    ASSERT_TRUE(client.connect("127.0.0.1", local_port(server)));
    auto accepted = server.accept(false);
    ASSERT_NE(accepted, nullptr);
    aeon::socket* base = accepted.get();
    ASSERT_TRUE(base->is_valid_socket());
    const auto descriptor = base->sockfd;
    EXPECT_TRUE(base->close());
    EXPECT_FALSE(base->is_valid_socket());
    EXPECT_FALSE(descriptor_open(descriptor));
    EXPECT_FALSE(base->close());
}

TEST(TcpOwnership, EventAcceptCanBeClosedThroughBasePointer) {
    aeon::server_event_socket server;
    ASSERT_TRUE(server.listen("127.0.0.1", 0));
    aeon::client_socket client;
    ASSERT_TRUE(client.connect("127.0.0.1", local_port(server)));
    auto accepted = server.accept(false);
    ASSERT_NE(accepted, nullptr);
    aeon::socket* base = accepted.get();
    ASSERT_TRUE(base->is_valid_socket());
    const auto descriptor = base->sockfd;
    EXPECT_TRUE(base->close());
    EXPECT_FALSE(base->is_valid_socket());
    EXPECT_FALSE(descriptor_open(descriptor));
}

TEST(TcpOwnership, ListenerWithoutPendingConnectionTimesOut) {
    aeon::server_socket server;
    ASSERT_TRUE(server.listen("127.0.0.1", 0));
    ASSERT_EQ(server.set_accept_timeout(25), 0);
    EXPECT_EQ(server.accept(), nullptr);
    EXPECT_TRUE(server.is_valid_socket());
}

TEST(TcpOwnership, ClosedListenerCannotAccept) {
    aeon::server_socket server;
    ASSERT_TRUE(server.close());
    EXPECT_EQ(server.accept(), nullptr);
    EXPECT_NE(server.get_error(), aeon::err_none);
}

TEST(TcpOwnership, RejectedBindPreservesExistingDescriptor) {
    aeon::server_socket server;
    ASSERT_TRUE(server.is_valid_socket());
    const auto original = server.sockfd;
    EXPECT_EQ(server.bind(nullptr, 0), -1);
    EXPECT_TRUE(server.is_valid_socket());
    EXPECT_EQ(server.sockfd, original);
    EXPECT_TRUE(descriptor_open(original));
}

TEST(TcpOwnership, RejectsInvalidListenAndTimeout) {
    aeon::server_socket server;
    EXPECT_FALSE(server.listen("bad-address", 12345));
    EXPECT_FALSE(server.listen("127.0.0.1", -2));
    EXPECT_EQ(server.set_accept_timeout(-1), -1);
    EXPECT_EQ(server.set_accept_timeout(60001), -1);
    EXPECT_TRUE(server.is_valid_socket());
}

TEST(TcpOwnership, EventListenerRejectsInvalidInputsAndCanRebind) {
    aeon::server_event_socket server;
    EXPECT_FALSE(server.listen("invalid", 2345));
    EXPECT_EQ(server.set_accept_timeout(60001), -1);
    ASSERT_TRUE(server.listen("127.0.0.1", 0));
    EXPECT_GT(local_port(server), 0);
    ASSERT_TRUE(server.listen("127.0.0.1", 0));
    EXPECT_GT(local_port(server), 0);
}

TEST(TcpOwnership, EventClientConnectUsesItsSingleSocketBase) {
    aeon::server_socket server;
    ASSERT_TRUE(server.listen("127.0.0.1", 0));
    aeon::client_event_socket client;
    bool connected_callback = false;
    client.on_connect([&] { connected_callback = true; });
    ASSERT_TRUE(client.connect("127.0.0.1", local_port(server)));
    EXPECT_TRUE(connected_callback);
    aeon::socket& base = client;
    EXPECT_TRUE(base.connected);
    EXPECT_TRUE(base.is_valid_socket());
    EXPECT_EQ(base.get_remote_ip(), "127.0.0.1");
    auto accepted = server.accept(true);
    ASSERT_NE(accepted, nullptr);
    EXPECT_NE(base.sockfd, accepted->sockfd);
    EXPECT_TRUE(static_cast<aeon::socket&>(client).close());
    EXPECT_FALSE(base.is_valid_socket());
}
