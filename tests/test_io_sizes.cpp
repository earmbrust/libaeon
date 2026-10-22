#include <gtest/gtest.h>
#include "aeon.hpp"

#include <array>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace {
int bound_port(const aeon::socket& socket) {
    sockaddr_storage bound{};
    socklen_t len = static_cast<socklen_t>(sizeof(bound));
    if (::getsockname(socket.sockfd, reinterpret_cast<sockaddr*>(&bound), &len) != 0)
        return -1;
    if (bound.ss_family == AF_INET)
        return ntohs(reinterpret_cast<const sockaddr_in*>(&bound)->sin_port);
    if (bound.ss_family == AF_INET6)
        return ntohs(reinterpret_cast<const sockaddr_in6*>(&bound)->sin6_port);
    return -1;
}

struct tcp_pair {
    aeon::server_socket listener;
    aeon::client_socket sender;
    std::unique_ptr<aeon::socket> receiver;

    bool start() {
        if (!listener.listen("127.0.0.1", 0)) return false;
        const int port = bound_port(listener);
        if (port <= 0 || !sender.connect("127.0.0.1", port)) return false;
        receiver = listener.accept(true);
        return receiver && receiver->set_read_timeout(2000) == 0;
    }
};

struct udp_pair {
    aeon::udp_server_socket listener;
    aeon::udp_client_socket sender;
    bool start() {
        if (!listener.listen(0)) return false;
        const int port = bound_port(listener);
        return port > 0 && sender.connect("127.0.0.1", port) &&
               listener.set_read_timeout(2000) == 0;
    }
};

std::string payload(std::size_t size) {
    std::string out(size, '\0');
    for (std::size_t i = 0; i < size; ++i) {
        out[i] = static_cast<char>((i * 17 + 31) % 256);
    }
    return out;
}
} // namespace

TEST(IoSizes, TcpReadUntilHandlesLargeBinaryPayload) {
    tcp_pair pair;
    ASSERT_TRUE(pair.start());
    const auto bytes = payload(4096);
    ASSERT_EQ(pair.sender.write(bytes.data(), static_cast<int>(bytes.size())), 4096);
    std::vector<char> received(bytes.size() + 2, '!');
    ASSERT_EQ(pair.receiver->read_until(received.data() + 1, 4096), 4096);
    EXPECT_EQ(received.front(), '!');
    EXPECT_EQ(received.back(), '!');
    EXPECT_EQ(std::string(received.data() + 1, 4096), bytes);
}

TEST(IoSizes, TcpReadUntilReturnsPartialBytesAtEof) {
    tcp_pair pair;
    ASSERT_TRUE(pair.start());
    const auto bytes = payload(513);
    ASSERT_EQ(pair.sender.write(bytes.data(), static_cast<int>(bytes.size())), 513);
    ASSERT_TRUE(pair.sender.close());
    std::vector<char> received(1024, '!');
    ASSERT_EQ(pair.receiver->read_until(received.data(), static_cast<int>(received.size())), 513);
    EXPECT_EQ(std::string(received.data(), 513), bytes);
    EXPECT_EQ(received[513], '!');
}

TEST(IoSizes, TcpReadStringPreservesNulsAndLargeContent) {
    tcp_pair pair;
    ASSERT_TRUE(pair.start());
    const auto bytes = payload(4096);
    ASSERT_EQ(pair.sender.write(bytes.data(), static_cast<int>(bytes.size())), 4096);
    ASSERT_TRUE(pair.sender.close());
    std::string result;
    while (result.size() < bytes.size()) {
        const std::string chunk = pair.receiver->read(4096);
        if (chunk.empty()) break;
        result += chunk;
    }
    EXPECT_EQ(result.size(), bytes.size());
    EXPECT_EQ(result, bytes);
}

TEST(IoSizes, TcpReadUntilOnNonblockingSocketReturnsZero) {
    tcp_pair pair;
    ASSERT_TRUE(pair.start());
    ASSERT_EQ(pair.receiver->set_blocking(false), 0);
    ASSERT_EQ(pair.receiver->set_read_timeout(0), 0);
    std::array<char, 512> buffer{};
    buffer.fill('!');
    EXPECT_EQ(pair.receiver->read_until(buffer.data(), static_cast<int>(buffer.size())), 0);
    EXPECT_EQ(buffer[0], '!');
    EXPECT_EQ(buffer[511], '!');
}

TEST(IoSizes, UdpExplicitBufferReadHonorsRequestedCapacity) {
    udp_pair pair;
    ASSERT_TRUE(pair.start());
    const auto bytes = payload(8192);
    ASSERT_EQ(pair.sender.write(bytes.data(), static_cast<int>(bytes.size())), 8192);
    std::vector<char> received(bytes.size() + 2, '!');
    ASSERT_EQ(pair.listener.read(received.data() + 1, 8192), 8192);
    EXPECT_EQ(received.front(), '!');
    EXPECT_EQ(received.back(), '!');
    EXPECT_EQ(std::string(received.data() + 1, 8192), bytes);
}

TEST(IoSizes, UdpStringReadPreservesNulsAndDatagramSize) {
    udp_pair pair;
    ASSERT_TRUE(pair.start());
    const auto bytes = payload(4096);
    ASSERT_EQ(pair.sender.write(bytes.data(), static_cast<int>(bytes.size())), 4096);
    const auto received = pair.listener.read(8192);
    EXPECT_EQ(received.size(), bytes.size());
    EXPECT_EQ(received, bytes);
    EXPECT_EQ(pair.listener.n, 4096);
}

TEST(IoSizes, UdpTimeoutPreservesExistingSocket) {
    udp_pair pair;
    ASSERT_TRUE(pair.start());
    ASSERT_EQ(pair.listener.set_read_timeout(20), 0);
    const auto fd = pair.listener.sockfd;
    EXPECT_TRUE(pair.listener.read(4096).empty());
    EXPECT_EQ(pair.listener.n, 0);
    EXPECT_EQ(pair.listener.sockfd, fd);
    EXPECT_TRUE(pair.listener.is_valid_socket());
}

TEST(IoSizes, InvalidReadSizesDoNotWritePastBuffer) {
    aeon::socket tcp;
    aeon::udp_socket udp;
    std::array<char, 2> buffer{'!', '!'};
    EXPECT_LT(tcp.read(buffer.data(), 0), 0);
    EXPECT_LT(udp.read(buffer.data(), -1), 0);
    EXPECT_EQ(buffer[0], '!');
    EXPECT_EQ(buffer[1], '!');
}
