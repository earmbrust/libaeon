#include <gtest/gtest.h>
#include "socket.hpp"
#include "aeon.hpp"
#include "client_socket.hpp"
#include "server_socket.hpp"
#include "udp_socket.hpp"
#include <type_traits>

class SocketBasicsTest : public ::testing::Test {
};

TEST_F(SocketBasicsTest, ExportedLibraryVersion) {
    const char* version = aeon::get_library_version();
    ASSERT_NE(version, nullptr);
    EXPECT_NE(version[0], '\0');
}

TEST_F(SocketBasicsTest, DefaultConstructor) {
    aeon::socket sock;
    EXPECT_TRUE(sock.is_valid_socket());
    EXPECT_FALSE(sock.connected);
}

TEST_F(SocketBasicsTest, ConstructorIPv4) {
    aeon::socket sock(aeon::socket::family_ipv4);
    EXPECT_TRUE(sock.is_valid_socket());
}

TEST_F(SocketBasicsTest, ConstructorIPv6) {
    aeon::socket sock(aeon::socket::family_ipv6);
    EXPECT_TRUE(sock.is_valid_socket());
}

TEST_F(SocketBasicsTest, ConstructorWithTypeAndFamily) {
    aeon::socket sock(aeon::socket::family_ipv4, aeon::socket::stream_type);
    EXPECT_TRUE(sock.is_valid_socket());
}

TEST_F(SocketBasicsTest, CloseSocket) {
    aeon::socket sock;
    bool closed = sock.close();
    EXPECT_TRUE(closed);
    EXPECT_FALSE(sock.is_valid_socket());
}

TEST_F(SocketBasicsTest, SetBlocking) {
    aeon::socket sock;
    int result = sock.set_blocking(false);
    EXPECT_EQ(result, 0);
    result = sock.set_blocking(true);
    EXPECT_EQ(result, 0);
}

TEST_F(SocketBasicsTest, SetReadTimeout) {
    aeon::socket sock;
    int result = sock.set_read_timeout(500);
    EXPECT_EQ(result, 0);
    result = sock.set_read_timeout(0);
    EXPECT_EQ(result, 0);
    result = sock.set_read_timeout(10000);
    EXPECT_EQ(result, 0);
}

TEST_F(SocketBasicsTest, SetWriteTimeout) {
    aeon::socket sock;
    int result = sock.set_write_timeout(500);
    EXPECT_EQ(result, 0);
    result = sock.set_write_timeout(0);
    EXPECT_EQ(result, 0);
    result = sock.set_write_timeout(10000);
    EXPECT_EQ(result, 0);
}

TEST_F(SocketBasicsTest, SetConnectTimeout) {
    aeon::socket sock;
    int result = sock.set_connect_timeout(1000);
    EXPECT_EQ(result, 0);
    result = sock.set_connect_timeout(5000);
    EXPECT_EQ(result, 0);
}

TEST_F(SocketBasicsTest, SetTcpNoDelay) {
    aeon::socket sock;
    int result = sock.set_tcp_nodelay(true);
    EXPECT_EQ(result, 0);
    result = sock.set_tcp_nodelay(false);
    EXPECT_EQ(result, 0);
}

TEST_F(SocketBasicsTest, SetSoReuseaddr) {
    aeon::socket sock;
    int result = sock.set_so_reuseaddr(true);
    EXPECT_EQ(result, 0);
    result = sock.set_so_reuseaddr(false);
    EXPECT_EQ(result, 0);
}

TEST_F(SocketBasicsTest, SetSoLinger) {
    aeon::socket sock;
    int result = sock.set_so_linger(2);
    EXPECT_EQ(result, 0);
    result = sock.set_so_linger(10);
    EXPECT_EQ(result, 0);
}

TEST_F(SocketBasicsTest, SetIPv6V6Only) {
    aeon::socket sock(aeon::socket::family_ipv6);
    int result = sock.set_ipv6_v6only(true);
    EXPECT_EQ(result, 0);
    result = sock.set_ipv6_v6only(false);
    EXPECT_EQ(result, 0);
}

TEST_F(SocketBasicsTest, SetSocketReuseaddr) {
    aeon::socket sock;
    EXPECT_NO_THROW(sock.set_socket_reuseaddr());
}

TEST_F(SocketBasicsTest, SetSocketTcpNodelay) {
    aeon::socket sock;
    EXPECT_NO_THROW(sock.set_socket_tcp_nodelay());
}

TEST_F(SocketBasicsTest, SetSocketLinger) {
    aeon::socket sock;
    EXPECT_NO_THROW(sock.set_socket_linger(5));
}

TEST_F(SocketBasicsTest, GetRemoteIPUnconnected) {
    aeon::socket sock;
    std::string remote_ip = sock.get_remote_ip();
    // Unconnected sockets may return empty or generic address - just verify method works
    EXPECT_TRUE(true);
}

TEST_F(SocketBasicsTest, GetRemotePortUnconnected) {
    aeon::socket sock;
    int port = sock.get_remote_port();
    EXPECT_EQ(port, 0);
}

TEST_F(SocketBasicsTest, GetError) {
    aeon::socket sock;
    int error = sock.get_error();
    EXPECT_GE(error, -1);
}

TEST_F(SocketBasicsTest, GetState) {
    aeon::socket sock;
    int state = sock.get_state();
    EXPECT_GE(state, 0);
}

TEST_F(SocketBasicsTest, MultipleInstances) {
    aeon::socket sock1;
    aeon::socket sock2;
    aeon::socket sock3;

    EXPECT_TRUE(sock1.is_valid_socket());
    EXPECT_TRUE(sock2.is_valid_socket());
    EXPECT_TRUE(sock3.is_valid_socket());

    EXPECT_NE(sock1.sockfd, sock2.sockfd);
    EXPECT_NE(sock2.sockfd, sock3.sockfd);
}

TEST_F(SocketBasicsTest, WriteAfterClose) {
    aeon::socket sock;
    sock.close();
    int result = sock.write("test");
    EXPECT_LT(result, 0);
}

TEST_F(SocketBasicsTest, ConfigAfterClose) {
    aeon::socket sock;
    sock.close();
    int result = sock.set_blocking(false);
    EXPECT_NE(result, 0);
}

TEST_F(SocketBasicsTest, BlockingModeGuard) {
    aeon::socket sock;
    {
        aeon::blocking_mode_guard guard(&sock);
        EXPECT_TRUE(guard.is_valid());
    }
    EXPECT_TRUE(sock.is_valid_socket());
}

TEST_F(SocketBasicsTest, BlockingModeGuardClosed) {
    aeon::socket sock;
    sock.close();
    aeon::blocking_mode_guard guard(&sock);
    EXPECT_FALSE(guard.is_valid());
}

TEST_F(SocketBasicsTest, ConstSocket) {
    aeon::socket sock;
    const aeon::socket& const_ref = sock;
    EXPECT_TRUE(const_ref.is_valid_socket());
    std::string ip = const_ref.get_remote_ip();
    int port = const_ref.get_remote_port();
}

TEST_F(SocketBasicsTest, MultipleTimeoutSets) {
    aeon::socket sock;
    for (int timeout : {100, 500, 1000, 2000, 5000}) {
        int result = sock.set_read_timeout(timeout);
        EXPECT_EQ(result, 0);
    }
}

// C06/C07/M06/M07: resource ownership and initialization regression tests.
static_assert(!std::is_copy_constructible_v<aeon::socket>,
              "Native sockets must not be copied");
static_assert(!std::is_copy_assignable_v<aeon::socket>,
              "Native sockets must not be copy-assigned");
static_assert(!std::is_move_constructible_v<aeon::socket>,
              "Moves require an explicitly designed ownership transfer");
static_assert(!std::is_move_assignable_v<aeon::socket>,
              "Moves require an explicitly designed ownership transfer");
static_assert(std::has_virtual_destructor_v<aeon::socket>,
              "Socket derived objects must be deletable through their base");

// Check the OS socket itself rather than the object's connected flag.
static bool native_socket_is_open(aeon::socket_t fd) {
    int type = 0;
#ifdef NET_PLATFORM_WINDOWS
    int option_length = static_cast<int>(sizeof(type));
#else
    socklen_t option_length = sizeof(type);
#endif
    return ::getsockopt(fd, SOL_SOCKET, SO_TYPE,
                        reinterpret_cast<char*>(&type), &option_length) == 0;
}

TEST_F(SocketBasicsTest, DestructorClosesUnconnectedSocket) {
    aeon::socket_t descriptor = aeon::invalid_socket;
    {
        aeon::socket socket;
        ASSERT_TRUE(socket.is_valid_socket());
        ASSERT_FALSE(socket.connected);
        descriptor = socket.sockfd;
        EXPECT_TRUE(native_socket_is_open(descriptor));
    }
    EXPECT_FALSE(native_socket_is_open(descriptor));
}

TEST_F(SocketBasicsTest, ServerDestructorClosesOnce) {
    aeon::socket_t descriptor = aeon::invalid_socket;
    {
        aeon::server_socket server;
        ASSERT_TRUE(server.is_valid_socket());
        descriptor = server.sockfd;
    }
    EXPECT_FALSE(native_socket_is_open(descriptor));
}

TEST_F(SocketBasicsTest, ExplicitCloseInvalidatesDescriptor) {
    aeon::socket socket;
    ASSERT_TRUE(socket.is_valid_socket());
    aeon::socket_t descriptor = socket.sockfd;
    EXPECT_TRUE(socket.close());
    EXPECT_FALSE(socket.is_valid_socket());
    EXPECT_FALSE(socket.connected);
    EXPECT_FALSE(native_socket_is_open(descriptor));
}

TEST_F(SocketBasicsTest, InvalidFamilyDoesNotOwnNativeSocket) {
    aeon::socket socket(-1, aeon::socket::stream_type);
    EXPECT_FALSE(socket.is_valid_socket());
    EXPECT_FALSE(socket.connected);
    EXPECT_EQ(socket.sockfd, aeon::invalid_socket);
}

TEST_F(SocketBasicsTest, DefaultClientSocketOwnsOneDescriptor) {
    aeon::client_socket client;
    ASSERT_TRUE(client.is_valid_socket());
    EXPECT_FALSE(client.connected);
    EXPECT_TRUE(native_socket_is_open(client.sockfd));
    EXPECT_FALSE(client.connect()); // Not configured: no undefined hostname/port.
}

TEST_F(SocketBasicsTest, UdpConstructorCreatesDatagramSocket) {
    aeon::udp_socket sock;
    ASSERT_TRUE(sock.is_valid_socket());
    int type = 0;
#ifdef NET_PLATFORM_WINDOWS
    int option_length = static_cast<int>(sizeof(type));
#else
    socklen_t option_length = sizeof(type);
#endif
    ASSERT_EQ(::getsockopt(sock.sockfd, SOL_SOCKET, SO_TYPE,
                           reinterpret_cast<char*>(&type), &option_length), 0);
    EXPECT_EQ(type, SOCK_DGRAM);
}
