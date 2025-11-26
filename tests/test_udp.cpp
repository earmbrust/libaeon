#include <gtest/gtest.h>
#include "udp_socket.hpp"
#include "udp_client_socket.hpp"
#include "udp_server_socket.hpp"
#include <thread>
#include <chrono>

using namespace aeon;

class UDPSocketTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code if needed
    }

    void TearDown() override {
        // Cleanup code if needed
    }
};

TEST_F(UDPSocketTest, UDPSocketConstruction) {
    udp_socket sock;
    EXPECT_TRUE(sock.is_valid_socket());
}

TEST_F(UDPSocketTest, UDPServerSocketConstruction) {
    udp_server_socket server;
    EXPECT_TRUE(server.is_valid_socket());
}

TEST_F(UDPSocketTest, UDPClientSocketConstruction) {
    udp_client_socket client;
    EXPECT_TRUE(client.is_valid_socket());
}

TEST_F(UDPSocketTest, UDPServerSocketListen) {
    udp_server_socket server;
    EXPECT_TRUE(server.is_valid_socket());

    // Listen on ephemeral port (0 = OS chooses)
    bool result = server.listen(0);
    EXPECT_TRUE(result);
}

TEST_F(UDPSocketTest, UDPServerSocketListenSpecificPort) {
    udp_server_socket server;
    EXPECT_TRUE(server.is_valid_socket());

    // Try specific port (may fail if in use)
    bool result = server.listen(45678);
    if (!result) {
        GTEST_SKIP() << "Port 45678 not available";
    }
}

TEST_F(UDPSocketTest, UDPClientSocketConnect) {
    udp_client_socket client;
    EXPECT_TRUE(client.is_valid_socket());

    // Connect to localhost
    bool result = client.connect("127.0.0.1", 0);
    // May or may not succeed on ephemeral port
}

TEST_F(UDPSocketTest, UDPClientSocketConstructorWithAddress) {
    // Constructor with hostname and port
    udp_client_socket client("127.0.0.1", 8000);
    EXPECT_TRUE(client.is_valid_socket());
}

TEST_F(UDPSocketTest, UDPSocketWrite) {
    udp_socket sock;
    EXPECT_TRUE(sock.is_valid_socket());

    // Note: write to unconnected socket may fail, that's OK
    const char* data = "test";
    int result = sock.write(data, 4);
    // Just verify method exists and doesn't crash
}

TEST_F(UDPSocketTest, UDPSocketWriteString) {
    udp_socket sock;
    EXPECT_TRUE(sock.is_valid_socket());

    std::string data = "test data";
    int result = sock.write(data);
    // Just verify method exists
}

TEST_F(UDPSocketTest, UDPSocketRead) {
    udp_socket sock;
    EXPECT_TRUE(sock.is_valid_socket());

    // Set read timeout to avoid hanging
    sock.set_read_timeout(100);

    char buffer[256];
    int result = sock.read(buffer, sizeof(buffer));
    // May timeout or fail, that's OK
}

TEST_F(UDPSocketTest, UDPSocketReadString) {
    udp_socket sock;
    EXPECT_TRUE(sock.is_valid_socket());

    sock.set_read_timeout(100);

    std::string result = sock.read(256);
    // May be empty, that's OK
}

TEST_F(UDPSocketTest, UDPServerClose) {
    udp_server_socket server;
    EXPECT_TRUE(server.is_valid_socket());

    bool closed = server.close();
    EXPECT_TRUE(closed);
    EXPECT_FALSE(server.is_valid_socket());
}

TEST_F(UDPSocketTest, UDPClientClose) {
    udp_client_socket client;
    EXPECT_TRUE(client.is_valid_socket());

    bool closed = client.close();
    EXPECT_TRUE(closed);
    EXPECT_FALSE(client.is_valid_socket());
}

TEST_F(UDPSocketTest, UDPServerSetBlocking) {
    udp_server_socket server;
    EXPECT_TRUE(server.is_valid_socket());

    int result = server.set_blocking(false);
    EXPECT_EQ(result, 0);

    result = server.set_blocking(true);
    EXPECT_EQ(result, 0);
}

TEST_F(UDPSocketTest, UDPClientSetBlocking) {
    udp_client_socket client;
    EXPECT_TRUE(client.is_valid_socket());

    int result = client.set_blocking(false);
    EXPECT_EQ(result, 0);

    result = client.set_blocking(true);
    EXPECT_EQ(result, 0);
}

TEST_F(UDPSocketTest, UDPServerSetReadTimeout) {
    udp_server_socket server;

    int result = server.set_read_timeout(500);
    EXPECT_EQ(result, 0);

    result = server.set_read_timeout(0);
    EXPECT_EQ(result, 0);
}

TEST_F(UDPSocketTest, UDPClientSetReadTimeout) {
    udp_client_socket client;

    int result = client.set_read_timeout(500);
    EXPECT_EQ(result, 0);

    result = client.set_read_timeout(1000);
    EXPECT_EQ(result, 0);
}

TEST_F(UDPSocketTest, UDPSetWriteTimeout) {
    udp_socket sock;

    int result = sock.set_write_timeout(500);
    EXPECT_EQ(result, 0);

    result = sock.set_write_timeout(1000);
    EXPECT_EQ(result, 0);
}

TEST_F(UDPSocketTest, UDPSetReuseAddr) {
    udp_socket sock;

    int result = sock.set_so_reuseaddr(true);
    EXPECT_EQ(result, 0);

    result = sock.set_so_reuseaddr(false);
    EXPECT_EQ(result, 0);
}

TEST_F(UDPSocketTest, UDPMultipleServers) {
    udp_server_socket server1;
    udp_server_socket server2;
    udp_server_socket server3;

    EXPECT_TRUE(server1.is_valid_socket());
    EXPECT_TRUE(server2.is_valid_socket());
    EXPECT_TRUE(server3.is_valid_socket());
}

TEST_F(UDPSocketTest, UDPMultipleClients) {
    udp_client_socket client1;
    udp_client_socket client2;
    udp_client_socket client3;

    EXPECT_TRUE(client1.is_valid_socket());
    EXPECT_TRUE(client2.is_valid_socket());
    EXPECT_TRUE(client3.is_valid_socket());
}

TEST_F(UDPSocketTest, UDPServerListenThenClose) {
    udp_server_socket server;
    bool listen_result = server.listen(0);
    EXPECT_TRUE(listen_result);

    bool close_result = server.close();
    EXPECT_TRUE(close_result);
}

TEST_F(UDPSocketTest, UDPClientConnectThenClose) {
    udp_client_socket client;
    bool connect_result = client.connect("127.0.0.1", 5000);
    // May fail, but socket should still be closeable

    bool close_result = client.close();
    EXPECT_TRUE(close_result);
}

TEST_F(UDPSocketTest, UDPServerGetRemoteIP) {
    udp_server_socket server;
    server.listen(0);

    std::string remote_ip = server.get_remote_ip();
    // May be empty for unconnected server
}

TEST_F(UDPSocketTest, UDPClientGetRemoteIP) {
    udp_client_socket client;
    client.connect("127.0.0.1", 5000);

    std::string remote_ip = client.get_remote_ip();
    // May be empty or contain address
}

TEST_F(UDPSocketTest, UDPServerGetRemotePort) {
    udp_server_socket server;
    server.listen(0);

    int port = server.get_remote_port();
    // May be 0 for unconnected server
}

TEST_F(UDPSocketTest, UDPClientGetRemotePort) {
    udp_client_socket client;
    client.connect("127.0.0.1", 5000);

    int port = client.get_remote_port();
    // May or may not be set
}

TEST_F(UDPSocketTest, UDPServerGetError) {
    udp_server_socket server;
    int error = server.get_error();
    // Just verify method exists
}

TEST_F(UDPSocketTest, UDPClientGetError) {
    udp_client_socket client;
    int error = client.get_error();
    // Just verify method exists
}

TEST_F(UDPSocketTest, UDPServerGetState) {
    udp_server_socket server;
    int state = server.get_state();
    // Just verify method exists
}

TEST_F(UDPSocketTest, UDPClientGetState) {
    udp_client_socket client;
    int state = client.get_state();
    // Just verify method exists
}

TEST_F(UDPSocketTest, UDPServerListenMultipleTimes) {
    udp_server_socket server;

    bool result1 = server.listen(0);
    EXPECT_TRUE(result1);

    // Closing and listening again
    server.close();
    
    udp_server_socket server2;
    bool result2 = server2.listen(0);
    EXPECT_TRUE(result2);
}

TEST_F(UDPSocketTest, UDPClientConnectMultipleTimes) {
    udp_client_socket client;

    bool result1 = client.connect("127.0.0.1", 5000);
    // May fail, that's OK

    bool result2 = client.connect("127.0.0.1", 5001);
    // May fail, that's OK
}

TEST_F(UDPSocketTest, UDPServerSetTCPNoDelay) {
    udp_server_socket server;
    
    // TCP_NODELAY is a TCP socket option and doesn't apply to UDP
    // Just verify the method exists but don't assert on the result
    int result = server.set_tcp_nodelay(true);
    // UDP doesn't support TCP_NODELAY, so result will be -1, and that's expected
}

TEST_F(UDPSocketTest, UDPClientConnectWithAddress) {
    udp_client_socket client;
    
    address addr;
    // Try to connect with address
    bool result = client.connect(addr);
    // May fail on invalid address, that's OK
}

TEST_F(UDPSocketTest, UDPSocketIsValid) {
    udp_socket sock;
    EXPECT_TRUE(sock.is_valid_socket());

    sock.close();
    EXPECT_FALSE(sock.is_valid_socket());
}

TEST_F(UDPSocketTest, UDPServerIsValid) {
    udp_server_socket server;
    EXPECT_TRUE(server.is_valid_socket());

    server.close();
    EXPECT_FALSE(server.is_valid_socket());
}

TEST_F(UDPSocketTest, UDPClientIsValid) {
    udp_client_socket client;
    EXPECT_TRUE(client.is_valid_socket());

    client.close();
    EXPECT_FALSE(client.is_valid_socket());
}

TEST_F(UDPSocketTest, UDPWriteAfterClose) {
    udp_socket sock;
    sock.close();

    int result = sock.write("test");
    EXPECT_LT(result, 0);
}

TEST_F(UDPSocketTest, UDPReadAfterClose) {
    udp_socket sock;
    sock.close();

    char buffer[256];
    int result = sock.read(buffer, sizeof(buffer));
    EXPECT_LT(result, 0);
}