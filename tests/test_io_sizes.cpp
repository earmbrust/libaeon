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
    aeon::socket* receiver = nullptr; // listener owns it

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

// M6j.4: Exercise each base TCP write overload through a real loopback peer.
// The small sends are deliberate: a successful stream send is not generally
// required to transmit every byte of a larger buffer.
TEST(IoSizes, TcpExplicitSizeWritesPreserveEmbeddedNuls) {
    tcp_pair pair;
    ASSERT_TRUE(pair.start());
    char mutable_bytes[] = {'A', '\0', 'B'};
    const char const_bytes[] = {'C', '\0', 'D'};
    std::array<char, 3> received{};

    ASSERT_EQ(pair.sender.write(mutable_bytes, 3), 3);
    ASSERT_EQ(pair.receiver->read_until(received.data(), 3), 3);
    EXPECT_EQ(std::string(received.data(), 3), std::string(mutable_bytes, 3));

    ASSERT_EQ(pair.sender.write(const_bytes, 3), 3);
    ASSERT_EQ(pair.receiver->read_until(received.data(), 3), 3);
    EXPECT_EQ(std::string(received.data(), 3), std::string(const_bytes, 3));
}

TEST(IoSizes, TcpCStringAndStringWritesUseTheirOwnLengths) {
    tcp_pair pair;
    ASSERT_TRUE(pair.start());
    char mutable_text[] = "one";
    const char const_text[] = "two";
    const std::string binary("Z\0Q", 3);
    std::array<char, 3> received{};

    ASSERT_EQ(pair.sender.write(mutable_text), 3);
    ASSERT_EQ(pair.receiver->read_until(received.data(), 3), 3);
    EXPECT_EQ(std::string(received.data(), 3), "one");

    ASSERT_EQ(pair.sender.write(const_text), 3);
    ASSERT_EQ(pair.receiver->read_until(received.data(), 3), 3);
    EXPECT_EQ(std::string(received.data(), 3), "two");

    ASSERT_EQ(pair.sender.write(binary), 3);
    ASSERT_EQ(pair.receiver->read_until(received.data(), 3), 3);
    EXPECT_EQ(std::string(received.data(), 3), binary);
}

TEST(IoSizes, TcpReadLineStopsAtCrLfWithoutConsumingNextLine) {
    tcp_pair pair;
    ASSERT_TRUE(pair.start());
    const char lines[] = "one\r\ntwo\r\n";
    ASSERT_EQ(pair.sender.write(lines, 10), 10);
    std::array<char, 32> buffer{};

    ASSERT_EQ(pair.receiver->read_line(buffer.data(), static_cast<int>(buffer.size())), 5);
    EXPECT_STREQ(buffer.data(), "one\r\n");
    ASSERT_EQ(pair.receiver->read_line(buffer.data(), static_cast<int>(buffer.size())), 5);
    EXPECT_STREQ(buffer.data(), "two\r\n");
}

TEST(IoSizes, TcpReadTimeoutThenDataThenEofUpdatesState) {
    tcp_pair pair;
    ASSERT_TRUE(pair.start());
    ASSERT_EQ(pair.receiver->set_read_timeout(30), 0);
    std::array<char, 8> buffer{};
    buffer.fill('!');

    EXPECT_EQ(pair.receiver->read(buffer.data(), static_cast<int>(buffer.size())), 0);
    EXPECT_EQ(pair.receiver->get_error(), aeon::err_timeout);
    EXPECT_EQ(pair.receiver->n, 0);
    EXPECT_TRUE(pair.receiver->connected);
    EXPECT_EQ(buffer[0], '!');

    ASSERT_EQ(pair.sender.write("OK", 2), 2);
    ASSERT_EQ(pair.receiver->read(buffer.data(), static_cast<int>(buffer.size())), 2);
    EXPECT_EQ(std::string(buffer.data(), 2), "OK");
    EXPECT_EQ(pair.receiver->get_error(), aeon::err_none);
    EXPECT_EQ(pair.receiver->n, 2);

    ASSERT_TRUE(pair.sender.close());
    EXPECT_EQ(pair.receiver->read(buffer.data(), static_cast<int>(buffer.size())), 0);
    EXPECT_EQ(pair.receiver->get_error(), aeon::err_none);
    EXPECT_FALSE(pair.receiver->connected);
}

TEST(IoSizes, TcpInvalidBuffersDoNotPreventSubsequentValidIo) {
    tcp_pair pair;
    ASSERT_TRUE(pair.start());
    std::array<char, 4> buffer{'!', '!', '!', '!'};

    EXPECT_EQ(pair.receiver->read(nullptr, 1), NET_SOCKET_ERROR);
    EXPECT_EQ(pair.receiver->get_error(), aeon::err_invalid_argument);
    EXPECT_EQ(pair.receiver->n, NET_SOCKET_ERROR);
    EXPECT_EQ(pair.receiver->read(buffer.data(), 0), NET_SOCKET_ERROR);
    EXPECT_EQ(pair.receiver->get_error(), aeon::err_invalid_argument);
    EXPECT_EQ(pair.receiver->read_until(nullptr, 1), NET_SOCKET_ERROR);
    EXPECT_EQ(pair.receiver->get_error(), aeon::err_invalid_argument);
    EXPECT_EQ(pair.sender.write(static_cast<char*>(nullptr), 1), NET_SOCKET_ERROR);
    EXPECT_EQ(pair.sender.get_error(), aeon::err_invalid_argument);
    EXPECT_EQ(pair.sender.write(std::string{}), NET_SOCKET_ERROR);
    EXPECT_EQ(pair.sender.get_error(), aeon::err_invalid_argument);
    EXPECT_EQ(buffer[0], '!');
    EXPECT_EQ(buffer[3], '!');

    ASSERT_EQ(pair.sender.write("x", 1), 1);
    EXPECT_EQ(pair.sender.get_error(), aeon::err_none);
    ASSERT_EQ(pair.receiver->read(buffer.data(), static_cast<int>(buffer.size())), 1);
    EXPECT_EQ(pair.receiver->get_error(), aeon::err_none);
    EXPECT_EQ(buffer[0], 'x');
    EXPECT_EQ(buffer[1], '!');
}

TEST(IoSizes, UdpWriteOverloadsKeepDatagramBoundariesAndBinaryData) {
    udp_pair pair;
    ASSERT_TRUE(pair.start());
    char mutable_text[] = "one";
    const char const_text[] = "two";
    const char binary[] = {'A', '\0', 'B'};
    std::array<char, 16> buffer{};

    ASSERT_EQ(pair.sender.write(mutable_text), 3);
    ASSERT_EQ(pair.listener.read(buffer.data(), static_cast<int>(buffer.size())), 3);
    EXPECT_EQ(std::string(buffer.data(), 3), "one");

    ASSERT_EQ(pair.sender.write(const_text), 3);
    ASSERT_EQ(pair.listener.read(buffer.data(), static_cast<int>(buffer.size())), 3);
    EXPECT_EQ(std::string(buffer.data(), 3), "two");

    ASSERT_EQ(pair.sender.write(binary, 3), 3);
    ASSERT_EQ(pair.listener.read(buffer.data(), static_cast<int>(buffer.size())), 3);
    EXPECT_EQ(std::string(buffer.data(), 3), std::string(binary, 3));

    char mutable_binary[] = {'X', '\0', 'Y'};
    ASSERT_EQ(pair.sender.write(mutable_binary, 3), 3);
    ASSERT_EQ(pair.listener.read(buffer.data(), static_cast<int>(buffer.size())), 3);
    EXPECT_EQ(std::string(buffer.data(), 3), std::string(mutable_binary, 3));

    const std::string text("Q\0R", 3);
    ASSERT_EQ(pair.sender.write(text), 3);
    ASSERT_EQ(pair.listener.read(buffer.data(), static_cast<int>(buffer.size())), 3);
    EXPECT_EQ(std::string(buffer.data(), 3), text);
}

TEST(IoSizes, UdpTimeoutAndEmptyDatagramRemainDistinguishableByError) {
    udp_pair pair;
    ASSERT_TRUE(pair.start());
    ASSERT_EQ(pair.listener.set_read_timeout(30), 0);
    const auto original_descriptor = pair.listener.sockfd;
    std::array<char, 8> buffer{};

    ASSERT_EQ(pair.listener.read(buffer.data(), static_cast<int>(buffer.size())), 0);
    EXPECT_EQ(pair.listener.get_error(), aeon::err_timeout);
    EXPECT_EQ(pair.listener.n, 0);
    EXPECT_EQ(pair.listener.sockfd, original_descriptor);

    ASSERT_EQ(pair.sender.write(std::string{}), 0);
    ASSERT_EQ(pair.listener.read(buffer.data(), static_cast<int>(buffer.size())), 0);
    EXPECT_EQ(pair.listener.get_error(), aeon::err_none);
    EXPECT_EQ(pair.listener.n, 0);
    EXPECT_TRUE(pair.listener.is_valid_socket());

    ASSERT_EQ(pair.sender.write("X", 1), 1);
    ASSERT_EQ(pair.listener.read(buffer.data(), static_cast<int>(buffer.size())), 1);
    EXPECT_EQ(buffer[0], 'X');
    EXPECT_EQ(pair.listener.get_error(), aeon::err_none);
    EXPECT_EQ(pair.listener.n, 1);
}

TEST(IoSizes, UdpInvalidWritesAndRecoveryHavePortableErrors) {
    udp_pair pair;
    ASSERT_TRUE(pair.start());
    EXPECT_EQ(pair.sender.write("bad", -1), NET_SOCKET_ERROR);
    EXPECT_EQ(pair.sender.get_error(), aeon::err_invalid_argument);
    EXPECT_EQ(pair.sender.write(static_cast<const char*>(nullptr), 1), NET_SOCKET_ERROR);
    EXPECT_EQ(pair.sender.get_error(), aeon::err_invalid_argument);
    EXPECT_EQ(pair.sender.write(static_cast<char*>(nullptr), 1), NET_SOCKET_ERROR);
    EXPECT_EQ(pair.sender.get_error(), aeon::err_invalid_argument);

    ASSERT_EQ(pair.sender.write("OK", 2), 2);
    EXPECT_EQ(pair.sender.get_error(), aeon::err_none);
    std::array<char, 4> received{};
    ASSERT_EQ(pair.listener.read(received.data(), static_cast<int>(received.size())), 2);
    EXPECT_EQ(std::string(received.data(), 2), "OK");
    EXPECT_EQ(pair.listener.get_error(), aeon::err_none);
}

TEST(IoSizes, TcpReadLineCapacityIsClampedToLegacyBuffer) {
    tcp_pair pair;
    ASSERT_TRUE(pair.start());
    const char message[] = "abc\r\n";
    ASSERT_EQ(pair.sender.write(message, 5), 5);
    std::vector<char> received(4098, '!');

    ASSERT_EQ(pair.receiver->read_line(received.data() + 1, 4096), 5);
    EXPECT_EQ(std::string(received.data() + 1), "abc\r\n");
    EXPECT_EQ(received.front(), '!');
    EXPECT_EQ(received.back(), '!');
    EXPECT_EQ(received[257], '!');
}

TEST(IoSizes, UdpLegacyReadTruncatesOnlyItsCurrentDatagram) {
    udp_pair pair;
    ASSERT_TRUE(pair.start());
    const auto bytes = payload(300);
    ASSERT_EQ(pair.sender.write(bytes.data(), static_cast<int>(bytes.size())), 300);
#ifdef NET_PLATFORM_WINDOWS
    // Winsock reports WSAEMSGSIZE instead of returning a truncated byte count.
    ASSERT_EQ(pair.listener.read(), NET_SOCKET_ERROR);
    EXPECT_EQ(pair.listener.n, NET_SOCKET_ERROR);
    EXPECT_EQ(pair.listener.get_error(), aeon::err_message_too_large);
#else
    ASSERT_EQ(pair.listener.read(), aeon::socket::max_buffer_size - 1);
    EXPECT_EQ(pair.listener.n, aeon::socket::max_buffer_size - 1);
    EXPECT_EQ(pair.listener.get_error(), aeon::err_none);
#endif

    ASSERT_EQ(pair.sender.write("Z", 1), 1);
    char next = '!';
    ASSERT_EQ(pair.listener.read(&next, 1), 1);
    EXPECT_EQ(next, 'Z');
    EXPECT_EQ(pair.listener.get_error(), aeon::err_none);
}

TEST(IoSizes, TcpClosedPeerRejectsReadAndWriteWithoutChangingBuffer) {
    tcp_pair pair;
    ASSERT_TRUE(pair.start());
    ASSERT_TRUE(pair.receiver->close());
    std::array<char, 4> buffer{'!', '!', '!', '!'};

    EXPECT_EQ(pair.receiver->read(buffer.data(), 4), NET_SOCKET_ERROR);
    EXPECT_EQ(pair.receiver->get_error(), aeon::err_no_socket);
    EXPECT_EQ(pair.receiver->n, NET_SOCKET_ERROR);
    EXPECT_EQ(pair.receiver->read_until(buffer.data(), 4), NET_SOCKET_ERROR);
    EXPECT_EQ(pair.receiver->get_error(), aeon::err_no_socket);
    EXPECT_EQ(pair.receiver->n, NET_SOCKET_ERROR);
    EXPECT_TRUE(pair.receiver->read(4).empty());
    EXPECT_EQ(pair.receiver->write("X", 1), NET_SOCKET_ERROR);
    EXPECT_EQ(pair.receiver->get_error(), aeon::err_no_socket);
    EXPECT_EQ(buffer, (std::array<char, 4>{'!', '!', '!', '!'}));
}

TEST(IoSizes, TcpReadUntilReturnsProgressOnTimeoutAndCanResume) {
    tcp_pair pair;
    ASSERT_TRUE(pair.start());
    ASSERT_EQ(pair.receiver->set_read_timeout(40), 0);
    ASSERT_EQ(pair.sender.write("part", 4), 4);

    char received[9]{};
    received[8] = '!';
    EXPECT_EQ(pair.receiver->read_until(received, 8), 4);
    EXPECT_EQ(std::string(received, 4), "part");
    EXPECT_EQ(received[4], '\0');
    EXPECT_EQ(received[8], '!');
    EXPECT_EQ(pair.receiver->get_error(), aeon::err_timeout);
    EXPECT_TRUE(pair.receiver->connected);

    ASSERT_EQ(pair.sender.write("rest", 4), 4);
    EXPECT_EQ(pair.receiver->read_until(received + 4, 4), 4);
    EXPECT_EQ(std::string(received, 8), "partrest");
    EXPECT_EQ(pair.receiver->get_error(), aeon::err_none);
    EXPECT_EQ(received[8], '!');
}
