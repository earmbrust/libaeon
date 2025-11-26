#include <gtest/gtest.h>
#include "socket.h"

class SocketBasicsTest : public ::testing::Test {
};

TEST_F(SocketBasicsTest, DefaultConstructor) {
    net::socket sock;
    EXPECT_TRUE(sock.is_valid_socket());
    EXPECT_FALSE(sock.connected);
}

TEST_F(SocketBasicsTest, ConstructorIPv4) {
    net::socket sock(net::socket::family_ipv4);
    EXPECT_TRUE(sock.is_valid_socket());
}

TEST_F(SocketBasicsTest, ConstructorIPv6) {
    net::socket sock(net::socket::family_ipv6);
    EXPECT_TRUE(sock.is_valid_socket());
}

TEST_F(SocketBasicsTest, ConstructorWithTypeAndFamily) {
    net::socket sock(net::socket::family_ipv4, net::socket::stream_type);
    EXPECT_TRUE(sock.is_valid_socket());
}

TEST_F(SocketBasicsTest, CloseSocket) {
    net::socket sock;
    bool closed = sock.close();
    EXPECT_TRUE(closed);
    EXPECT_FALSE(sock.is_valid_socket());
}

TEST_F(SocketBasicsTest, SetBlocking) {
    net::socket sock;
    int result = sock.set_blocking(false);
    EXPECT_EQ(result, 0);
    result = sock.set_blocking(true);
    EXPECT_EQ(result, 0);
}

TEST_F(SocketBasicsTest, SetReadTimeout) {
    net::socket sock;
    int result = sock.set_read_timeout(500);
    EXPECT_EQ(result, 0);
    result = sock.set_read_timeout(0);
    EXPECT_EQ(result, 0);
    result = sock.set_read_timeout(10000);
    EXPECT_EQ(result, 0);
}

TEST_F(SocketBasicsTest, SetWriteTimeout) {
    net::socket sock;
    int result = sock.set_write_timeout(500);
    EXPECT_EQ(result, 0);
    result = sock.set_write_timeout(0);
    EXPECT_EQ(result, 0);
    result = sock.set_write_timeout(10000);
    EXPECT_EQ(result, 0);
}

TEST_F(SocketBasicsTest, SetConnectTimeout) {
    net::socket sock;
    int result = sock.set_connect_timeout(1000);
    EXPECT_EQ(result, 0);
    result = sock.set_connect_timeout(5000);
    EXPECT_EQ(result, 0);
}

TEST_F(SocketBasicsTest, SetTcpNoDelay) {
    net::socket sock;
    int result = sock.set_tcp_nodelay(true);
    EXPECT_EQ(result, 0);
    result = sock.set_tcp_nodelay(false);
    EXPECT_EQ(result, 0);
}

TEST_F(SocketBasicsTest, SetSoReuseaddr) {
    net::socket sock;
    int result = sock.set_so_reuseaddr(true);
    EXPECT_EQ(result, 0);
    result = sock.set_so_reuseaddr(false);
    EXPECT_EQ(result, 0);
}

TEST_F(SocketBasicsTest, SetSoLinger) {
    net::socket sock;
    int result = sock.set_so_linger(2);
    EXPECT_EQ(result, 0);
    result = sock.set_so_linger(10);
    EXPECT_EQ(result, 0);
}

TEST_F(SocketBasicsTest, SetIPv6V6Only) {
    net::socket sock(net::socket::family_ipv6);
    int result = sock.set_ipv6_v6only(true);
    EXPECT_EQ(result, 0);
    result = sock.set_ipv6_v6only(false);
    EXPECT_EQ(result, 0);
}

TEST_F(SocketBasicsTest, SetSocketReuseaddr) {
    net::socket sock;
    EXPECT_NO_THROW(sock.set_socket_reuseaddr());
}

TEST_F(SocketBasicsTest, SetSocketTcpNodelay) {
    net::socket sock;
    EXPECT_NO_THROW(sock.set_socket_tcp_nodelay());
}

TEST_F(SocketBasicsTest, SetSocketLinger) {
    net::socket sock;
    EXPECT_NO_THROW(sock.set_socket_linger(5));
}

TEST_F(SocketBasicsTest, GetRemoteIPUnconnected) {
    net::socket sock;
    std::string remote_ip = sock.get_remote_ip();
    EXPECT_FALSE(remote_ip.empty() || remote_ip.empty()); // May be empty or not
}

TEST_F(SocketBasicsTest, GetRemotePortUnconnected) {
    net::socket sock;
    int port = sock.get_remote_port();
    EXPECT_EQ(port, 0);
}

TEST_F(SocketBasicsTest, GetError) {
    net::socket sock;
    int error = sock.get_error();
    EXPECT_GE(error, -1);
}

TEST_F(SocketBasicsTest, GetState) {
    net::socket sock;
    int state = sock.get_state();
    EXPECT_GE(state, 0);
}

TEST_F(SocketBasicsTest, MultipleInstances) {
    net::socket sock1;
    net::socket sock2;
    net::socket sock3;

    EXPECT_TRUE(sock1.is_valid_socket());
    EXPECT_TRUE(sock2.is_valid_socket());
    EXPECT_TRUE(sock3.is_valid_socket());

    EXPECT_NE(sock1.sockfd, sock2.sockfd);
    EXPECT_NE(sock2.sockfd, sock3.sockfd);
}

TEST_F(SocketBasicsTest, WriteAfterClose) {
    net::socket sock;
    sock.close();
    int result = sock.write("test");
    EXPECT_LT(result, 0);
}

TEST_F(SocketBasicsTest, ConfigAfterClose) {
    net::socket sock;
    sock.close();
    int result = sock.set_blocking(false);
    EXPECT_NE(result, 0);
}

TEST_F(SocketBasicsTest, BlockingModeGuard) {
    net::socket sock;
    {
        net::blocking_mode_guard guard(&sock);
        EXPECT_TRUE(guard.is_valid());
    }
    EXPECT_TRUE(sock.is_valid_socket());
}

TEST_F(SocketBasicsTest, BlockingModeGuardClosed) {
    net::socket sock;
    sock.close();
    net::blocking_mode_guard guard(&sock);
    EXPECT_FALSE(guard.is_valid());
}

TEST_F(SocketBasicsTest, ConstSocket) {
    net::socket sock;
    const net::socket& const_ref = sock;
    EXPECT_TRUE(const_ref.is_valid_socket());
    std::string ip = const_ref.get_remote_ip();
    int port = const_ref.get_remote_port();
}

TEST_F(SocketBasicsTest, MultipleTimeoutSets) {
    net::socket sock;
    for (int timeout : {100, 500, 1000, 2000, 5000}) {
        int result = sock.set_read_timeout(timeout);
        EXPECT_EQ(result, 0);
    }
}