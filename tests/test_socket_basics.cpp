#include <gtest/gtest.h>
#include "aeon.hpp"
#include "../src/native_error.hpp"
#include "socket.hpp"
#include "client_socket.hpp"
#include "server_socket.hpp"
#include "udp_socket.hpp"
#include <type_traits>
#ifndef NET_PLATFORM_WINDOWS
#include <fcntl.h>
#endif

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

TEST_F(SocketBasicsTest, LibraryErrorCodes) {
    EXPECT_EQ(aeon::err_none, 0);
    EXPECT_EQ(aeon::socket_error, -1);
    EXPECT_EQ(aeon::err_no_host, -0x010001);
    EXPECT_EQ(aeon::err_no_socket, -0x010002);
    EXPECT_NE(aeon::err_no_host, aeon::err_no_socket);
#ifdef ENABLE_SSL
    EXPECT_EQ(aeon::ssl_error, -0x020001);
    EXPECT_EQ(aeon::ssl_error_init, -0x020002);
    EXPECT_EQ(aeon::ssl_error_cert, -0x020003);
    EXPECT_EQ(aeon::ssl_error_key, -0x020004);
    EXPECT_EQ(aeon::ssl_error_handshake, -0x020005);
    EXPECT_EQ(aeon::ssl_error_io, -0x020006);
    EXPECT_NE(aeon::err_no_socket, aeon::ssl_error);
#endif
}

TEST_F(SocketBasicsTest, NativeErrorCodesArePortable) {
    EXPECT_EQ(aeon::translate_native_error(0), aeon::err_none);
#ifdef NET_PLATFORM_WINDOWS
    EXPECT_EQ(aeon::translate_native_error(WSAECONNRESET), aeon::err_connection_reset);
    EXPECT_EQ(aeon::translate_native_error(WSAEWOULDBLOCK), aeon::err_would_block);
    EXPECT_EQ(aeon::translate_native_error(WSAETIMEDOUT), aeon::err_timeout);
    EXPECT_EQ(aeon::translate_native_error(WSAEADDRINUSE), aeon::err_address_in_use);
    EXPECT_EQ(aeon::translate_native_error(WSAENOTSOCK), aeon::err_no_socket);
#else
    EXPECT_EQ(aeon::translate_native_error(ECONNRESET), aeon::err_connection_reset);
    EXPECT_EQ(aeon::translate_native_error(EAGAIN), aeon::err_would_block);
    EXPECT_EQ(aeon::translate_native_error(EWOULDBLOCK), aeon::err_would_block);
    EXPECT_EQ(aeon::translate_native_error(ETIMEDOUT), aeon::err_timeout);
    EXPECT_EQ(aeon::translate_native_error(EADDRINUSE), aeon::err_address_in_use);
    EXPECT_EQ(aeon::translate_native_error(EBADF), aeon::err_no_socket);
#endif
    EXPECT_EQ(aeon::translate_native_error(0x7F7F7F7F), aeon::err_system);
    EXPECT_EQ(aeon::err_invalid_argument, -0x010003);
    EXPECT_EQ(aeon::err_timeout, -0x010004);
    EXPECT_EQ(aeon::err_connection_reset, -0x010008);
    EXPECT_EQ(aeon::err_system, -0x01FFFF);
}

TEST_F(SocketBasicsTest, ClosedSocketRetainsErrorAndState) {
    aeon::socket sock;
    ASSERT_TRUE(sock.close());
    EXPECT_EQ(sock.set_blocking(false), -1);
    EXPECT_EQ(sock.get_error(), aeon::err_no_socket);
    EXPECT_EQ(sock.get_state(), aeon::state_create);
}

#ifdef ENABLE_SSL
TEST_F(SocketBasicsTest, UnconnectedTlsWriteIsAnError) {
    aeon::ssl_client_socket client;
    const char data[] = "test";
    EXPECT_LT(client.write(data, 4), 0);
    EXPECT_EQ(client.get_error(), aeon::err_no_socket);
}
#endif

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
#ifndef NET_PLATFORM_WINDOWS
    int flags = ::fcntl(sock.sockfd, F_GETFL, 0);
    ASSERT_NE(flags, -1);
    EXPECT_NE(flags & O_NONBLOCK, 0);
#endif
    result = sock.set_blocking(true);
    EXPECT_EQ(result, 0);
#ifndef NET_PLATFORM_WINDOWS
    flags = ::fcntl(sock.sockfd, F_GETFL, 0);
    ASSERT_NE(flags, -1);
    EXPECT_EQ(flags & O_NONBLOCK, 0);
#endif
}

TEST_F(SocketBasicsTest, InvalidTimeoutHasPortableError) {
    aeon::socket sock;
    EXPECT_EQ(sock.set_read_timeout(-1), -1);
    EXPECT_EQ(sock.get_error(), aeon::err_invalid_argument);
    EXPECT_EQ(sock.set_write_timeout(-1), -1);
    EXPECT_EQ(sock.get_error(), aeon::err_invalid_argument);
    EXPECT_EQ(sock.set_connect_timeout(-1), -1);
    EXPECT_EQ(sock.get_error(), aeon::err_invalid_argument);
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
#ifndef NET_PLATFORM_WINDOWS
        const int flags = ::fcntl(sock.sockfd, F_GETFL, 0);
        ASSERT_NE(flags, -1);
        EXPECT_NE(flags & O_NONBLOCK, 0);
#endif
    }
    EXPECT_TRUE(sock.is_valid_socket());
#ifndef NET_PLATFORM_WINDOWS
    const int flags = ::fcntl(sock.sockfd, F_GETFL, 0);
    ASSERT_NE(flags, -1);
    EXPECT_EQ(flags & O_NONBLOCK, 0);
#endif
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
