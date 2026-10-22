#include <gtest/gtest.h>
#include "aeon.hpp"

#include <cstring>
#include <memory>
#include <type_traits>
#include <utility>

namespace {

static_assert(std::is_base_of_v<aeon::socket, aeon::udp_event_socket>);
static_assert(std::is_base_of_v<aeon::udp_socket, aeon::udp_event_socket>);
static_assert(std::is_base_of_v<aeon::udp_event_socket, aeon::udp_client_event_socket>);
static_assert(std::is_base_of_v<aeon::udp_event_socket, aeon::udp_server_event_socket>);
static_assert(std::is_convertible_v<aeon::udp_client_event_socket*, aeon::socket*>);
static_assert(std::is_convertible_v<aeon::udp_server_event_socket*, aeon::socket*>);
static_assert(!std::is_base_of_v<aeon::udp_client_socket, aeon::udp_client_event_socket>);
static_assert(!std::is_base_of_v<aeon::udp_server_socket, aeon::udp_server_event_socket>);
static_assert(!std::is_copy_constructible_v<aeon::udp_event_socket>);
static_assert(!std::is_move_constructible_v<aeon::udp_event_socket>);

bool descriptor_open(aeon::socket_t fd) {
    int kind = 0;
#ifdef NET_PLATFORM_WINDOWS
    int length = sizeof(kind);
#else
    socklen_t length = sizeof(kind);
#endif
    return ::getsockopt(fd, SOL_SOCKET, SO_TYPE,
                        reinterpret_cast<char*>(&kind), &length) == 0;
}

int bound_port(const aeon::socket& owner) {
    sockaddr_storage addr{};
    socklen_t length = sizeof(addr);
    if (::getsockname(owner.sockfd, reinterpret_cast<sockaddr*>(&addr), &length) != 0)
        return -1;
    if (addr.ss_family == AF_INET) {
        return ntohs(reinterpret_cast<const sockaddr_in*>(&addr)->sin_port);
    }
    if (addr.ss_family == AF_INET6) {
        return ntohs(reinterpret_cast<const sockaddr_in6*>(&addr)->sin6_port);
    }
    return -1;
}

#ifdef NET_PLATFORM_LINUX
bool native_nonblocking(aeon::socket_t fd) {
    const int flags = fcntl(fd, F_GETFL, 0);
    return flags >= 0 && (flags & O_NONBLOCK) != 0;
}
#endif

} // namespace

TEST(UdpOwnership, EventTransportHasExactlyOneSocketOwner) {
    aeon::socket_t descriptor = aeon::invalid_socket;
    {
        aeon::udp_client_event_socket client;
        aeon::socket& base = client;
        ASSERT_TRUE(base.is_valid_socket());
        descriptor = base.sockfd;
        EXPECT_TRUE(descriptor_open(descriptor));
        EXPECT_FALSE(base.connected);
#ifdef NET_PLATFORM_LINUX
        EXPECT_TRUE(native_nonblocking(descriptor));
#endif
    }
    EXPECT_FALSE(descriptor_open(descriptor));
}

TEST(UdpOwnership, EventListenerHasExactlyOneSocketOwner) {
    aeon::socket_t descriptor = aeon::invalid_socket;
    {
        aeon::udp_server_event_socket server;
        aeon::socket& base = server;
        ASSERT_TRUE(base.is_valid_socket());
        descriptor = base.sockfd;
        EXPECT_TRUE(descriptor_open(descriptor));
    }
    EXPECT_FALSE(descriptor_open(descriptor));
}

TEST(UdpOwnership, EventBaseCloseIsVirtualAndIdempotent) {
    std::unique_ptr<aeon::socket> owner = std::make_unique<aeon::udp_server_event_socket>();
    ASSERT_TRUE(owner->is_valid_socket());
    const auto descriptor = owner->sockfd;
    EXPECT_TRUE(owner->close());
    EXPECT_FALSE(owner->close());
    EXPECT_FALSE(owner->is_valid_socket());
    EXPECT_FALSE(descriptor_open(descriptor));
    owner.reset();
}

TEST(UdpOwnership, InvalidClientDestinationDoesNotDestroyExistingSocket) {
    aeon::udp_client_event_socket client;
    ASSERT_TRUE(client.is_valid_socket());
    const auto descriptor = client.sockfd;
    EXPECT_FALSE(client.connect(nullptr, 1234));
    EXPECT_FALSE(client.connect("", 1234));
    EXPECT_FALSE(client.connect("127.0.0.1", -1));
    EXPECT_FALSE(client.connect("127.0.0.1", 65536));
    EXPECT_FALSE(client.connect());
    EXPECT_EQ(client.sockfd, descriptor);
    EXPECT_TRUE(descriptor_open(descriptor));
    EXPECT_FALSE(client.connected);
    EXPECT_NE(client.get_error(), aeon::err_none);
}

TEST(UdpOwnership, InvalidListenerPortPreservesNativeDescriptor) {
    aeon::udp_server_event_socket server;
    ASSERT_TRUE(server.listen(0));
    const auto descriptor = server.sockfd;
    const int port = bound_port(server);
    EXPECT_GT(port, 0);
    EXPECT_FALSE(server.listen(-1));
    EXPECT_FALSE(server.listen(65536));
    EXPECT_EQ(server.sockfd, descriptor);
    EXPECT_EQ(bound_port(server), port);
    EXPECT_TRUE(descriptor_open(descriptor));
}

TEST(UdpOwnership, EventListenerRebindsWithoutAnExtraOwnedDescriptor) {
    aeon::udp_server_event_socket server;
    ASSERT_TRUE(server.listen(0));
    const auto original = server.sockfd;
    ASSERT_TRUE(server.listen(0));
    ASSERT_GT(bound_port(server), 0);
    EXPECT_TRUE(descriptor_open(server.sockfd));
    if (original != server.sockfd) EXPECT_FALSE(descriptor_open(original));
}

TEST(UdpOwnership, PlainListenerAndEventListenerShareBindSemantics) {
    aeon::udp_server_socket plain;
    aeon::udp_server_event_socket event;
    ASSERT_TRUE(plain.listen(0));
    ASSERT_TRUE(event.listen(0));
    EXPECT_GT(bound_port(plain), 0);
    EXPECT_GT(bound_port(event), 0);
    EXPECT_NE(plain.sockfd, event.sockfd);
    EXPECT_TRUE(descriptor_open(plain.sockfd));
    EXPECT_TRUE(descriptor_open(event.sockfd));
}

TEST(UdpOwnership, ClientSelectsRemoteDestinationWithoutCreatingTcpSession) {
    aeon::udp_client_event_socket client;
    const auto descriptor = client.sockfd;
    ASSERT_TRUE(client.connect("127.0.0.1", 9999));
    EXPECT_EQ(client.sockfd, descriptor);
    EXPECT_TRUE(client.connected);
    EXPECT_EQ(client.get_remote_port(), 9999);
    EXPECT_EQ(client.get_remote_ip(), "127.0.0.1");
    EXPECT_TRUE(client.connect());
    EXPECT_EQ(client.sockfd, descriptor);
}

TEST(UdpOwnership, PlainAndEventClientsBothSupportAddressConnect) {
    sockaddr_in endpoint{};
    endpoint.sin_family = AF_INET;
    endpoint.sin_port = htons(12345);
    endpoint.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    aeon::address destination(endpoint);
    aeon::udp_client_socket plain;
    aeon::udp_client_event_socket event;
    ASSERT_TRUE(plain.connect(destination));
    ASSERT_TRUE(event.connect(destination));
    EXPECT_EQ(plain.get_remote_port(), 12345);
    EXPECT_EQ(event.get_remote_port(), 12345);
    EXPECT_EQ(plain.get_remote_ip(), event.get_remote_ip());
}

TEST(UdpOwnership, EventClientCanSendDatagramToPlainServer) {
    aeon::udp_server_socket server;
    ASSERT_TRUE(server.listen(0));
    ASSERT_EQ(server.set_read_timeout(250), 0);
    aeon::udp_client_event_socket client;
    ASSERT_TRUE(client.connect("127.0.0.1", bound_port(server)));
    ASSERT_EQ(client.write("hello", 5), 5);
    char message[16]{};
    ASSERT_EQ(server.read(message, sizeof(message)), 5);
    EXPECT_EQ(std::memcmp(message, "hello", 5), 0);
    EXPECT_GT(server.get_remote_port(), 0);
}

TEST(UdpOwnership, PlainClientCanSendDatagramToEventServer) {
    aeon::udp_server_event_socket server;
    ASSERT_TRUE(server.listen(0));
    ASSERT_EQ(server.set_read_timeout(250), 0);
    aeon::udp_client_socket client;
    ASSERT_TRUE(client.connect("127.0.0.1", bound_port(server)));
    ASSERT_EQ(client.write("data", 4), 4);
    char message[16]{};
    ASSERT_EQ(server.read(message, sizeof(message)), 4);
    EXPECT_EQ(std::memcmp(message, "data", 4), 0);
}

TEST(UdpOwnership, ServerRepliesToLastSenderWithoutAnotherSocket) {
    aeon::udp_server_socket server;
    ASSERT_TRUE(server.listen(0));
    aeon::udp_client_event_socket client;
    ASSERT_TRUE(client.connect("127.0.0.1", bound_port(server)));
    ASSERT_EQ(server.set_read_timeout(250), 0);
    ASSERT_EQ(client.set_read_timeout(250), 0);
    ASSERT_EQ(client.write("a", 1), 1);
    char received[16]{};
    ASSERT_EQ(server.read(received, sizeof(received)), 1);
    EXPECT_EQ(server.write("b", 1), 1);
    ASSERT_EQ(client.read(received, sizeof(received)), 1);
    EXPECT_EQ(received[0], 'b');
}

TEST(UdpOwnership, FamilySwitchKeepsOneOwnedDescriptor) {
    aeon::udp_client_event_socket client;
    const auto original = client.sockfd;
    sockaddr_in6 ipv6{};
    ipv6.sin6_family = AF_INET6;
    ipv6.sin6_addr = in6addr_loopback;
    ipv6.sin6_port = htons(7777);
    if (!client.connect(aeon::address(ipv6))) {
        GTEST_SKIP() << "IPv6 UDP sockets unavailable";
    }
    EXPECT_TRUE(descriptor_open(client.sockfd));
    EXPECT_NE(client.sockfd, original);
    EXPECT_FALSE(descriptor_open(original));
#ifdef NET_PLATFORM_LINUX
    EXPECT_TRUE(native_nonblocking(client.sockfd));
#endif
    ASSERT_TRUE(client.connect("127.0.0.1", 7777));
    EXPECT_TRUE(descriptor_open(client.sockfd));
#ifdef NET_PLATFORM_LINUX
    EXPECT_TRUE(native_nonblocking(client.sockfd));
#endif
}

TEST(UdpOwnership, SocketConstructionDoesNotPermitCopyingNativeOwner) {
    EXPECT_FALSE(std::is_copy_constructible_v<aeon::udp_server_event_socket>);
    EXPECT_FALSE(std::is_move_constructible_v<aeon::udp_client_event_socket>);
}

TEST(UdpOwnership, InvalidAddressDoesNotModifyExistingDestination) {
    aeon::udp_client_socket client;
    ASSERT_TRUE(client.connect("127.0.0.1", 19001));
    const auto descriptor = client.sockfd;
    EXPECT_FALSE(client.connect(aeon::address()));
    EXPECT_EQ(client.sockfd, descriptor);
    EXPECT_EQ(client.get_remote_port(), 19001);
    EXPECT_TRUE(client.connected);
}

TEST(UdpOwnership, OrdinaryUdpDestructorClosesExactlyItsDescriptor) {
    aeon::socket_t native = aeon::invalid_socket;
    {
        aeon::udp_server_socket server;
        ASSERT_TRUE(server.listen(0));
        native = server.sockfd;
        EXPECT_TRUE(descriptor_open(native));
    }
    EXPECT_FALSE(descriptor_open(native));
}

TEST(UdpOwnership, EventServerRemainsNonblockingAfterSocketReplacement) {
    aeon::udp_server_event_socket server;
    ASSERT_TRUE(server.listen(0));
#ifdef NET_PLATFORM_LINUX
    EXPECT_TRUE(native_nonblocking(server.sockfd));
#endif
    ASSERT_TRUE(server.listen(0));
#ifdef NET_PLATFORM_LINUX
    EXPECT_TRUE(native_nonblocking(server.sockfd));
#endif
    EXPECT_GT(bound_port(server), 0);
}
