#include <gtest/gtest.h>
#include "socket.hpp"

class SocketBasicsTest : public ::testing::Test {
};

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
    EXPECT_FALSE(remote_ip.empty() || remote_ip.empty()); // May be empty or not
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