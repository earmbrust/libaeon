#include <gtest/gtest.h>
#include "aeon.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {
int local_port(const aeon::socket& owner) {
    sockaddr_in address{};
    socklen_t len = static_cast<socklen_t>(sizeof(address));
    if (::getsockname(owner.sockfd, reinterpret_cast<sockaddr*>(&address), &len) != 0)
        return -1;
    return ntohs(address.sin_port);
}

struct write_pair {
    aeon::server_event_socket listener;
    aeon::client_socket reader;
    aeon::event_socket* writer = nullptr; // listener owns it
    aeon::event_socket_set set;

    bool start() {
        if (!listener.listen("127.0.0.1", 0)) return false;
        if (!reader.connect("127.0.0.1", local_port(listener))) return false;
        writer = listener.accept(false);
        return writer && set.add(writer) && reader.set_read_timeout(2000) == 0;
    }
    ~write_pair() {
        if (writer) set.remove_socket(writer);
    }
};

std::string binary(std::size_t size) {
    std::string bytes(size, '\0');
    for (std::size_t i = 0; i < size; ++i) bytes[i] = static_cast<char>((i * 41 + 7) & 255);
    return bytes;
}
} // namespace

TEST(WriteQueue, RequiresConnectedNonblockingTcpStream) {
    aeon::event_socket unopened;
    EXPECT_FALSE(unopened.queue_write("x", 1));
    EXPECT_FALSE(unopened.queue_write("", 0));
    write_pair pair;
    ASSERT_TRUE(pair.start());
    EXPECT_FALSE(pair.writer->queue_write(nullptr, 1));
    ASSERT_EQ(pair.writer->set_blocking(true), 0);
    EXPECT_FALSE(pair.writer->queue_write("x", 1));
    ASSERT_EQ(pair.writer->set_blocking(false), 0);
    EXPECT_TRUE(pair.writer->queue_write("", 0));
    EXPECT_EQ(pair.writer->queued_bytes(), 0u);
}

TEST(WriteQueue, BoundsAreAtomicAndConfigurable) {
    write_pair pair;
    ASSERT_TRUE(pair.start());
    ASSERT_TRUE(pair.writer->set_max_queued_bytes(5));
    ASSERT_TRUE(pair.writer->queue_write("abcd", 4));
    EXPECT_FALSE(pair.writer->queue_write("xy", 2));
    EXPECT_FALSE(pair.writer->set_max_queued_bytes(3));
    EXPECT_EQ(pair.writer->queued_bytes(), 4u);
    EXPECT_TRUE(pair.writer->queue_write("e", 1));
    EXPECT_FALSE(pair.writer->queue_write("f", 1));
    EXPECT_EQ(pair.writer->queued_bytes(), 5u);
    ASSERT_GT(pair.set.poll(2000), 0);
    EXPECT_EQ(pair.writer->queued_bytes(), 0u);
    char received[5]{};
    ASSERT_EQ(pair.reader.read_until(received, 5), 5);
    EXPECT_EQ(std::string(received, 5), "abcde");
}

TEST(WriteQueue, BinaryDataIsFullyDeliveredWithBothDrainCallbacks) {
    write_pair pair;
    ASSERT_TRUE(pair.start());
    auto bytes = binary(8192);
    int local = 0, global = 0;
    pair.writer->on_drain([&] { ++local; });
    pair.set.on_socket_drain([&](aeon::event_socket* socket) {
        EXPECT_EQ(socket, pair.writer); ++global;
    });
    ASSERT_TRUE(pair.writer->queue_write(bytes));
    EXPECT_EQ(pair.writer->queued_bytes(), bytes.size());
    ASSERT_GT(pair.set.poll(2000), 0);
    EXPECT_EQ(pair.writer->queued_bytes(), 0u);
    EXPECT_EQ(local, 1);
    EXPECT_EQ(global, 1);
    std::string received(bytes.size(), '\0');
    ASSERT_EQ(pair.reader.read_until(received.data(), static_cast<int>(received.size())),
              static_cast<int>(received.size()));
    EXPECT_EQ(received, bytes);
    EXPECT_EQ(pair.set.poll(0), 0);
    EXPECT_EQ(local, 1);
}

TEST(WriteQueue, SmallWritesCoalesceWithoutLosingOrder) {
    write_pair pair;
    ASSERT_TRUE(pair.start());
    std::string expected;
    expected.reserve(1024);
    for (int i = 0; i < 1024; ++i) {
        char ch = static_cast<char>((i * 17) & 255);
        expected += ch;
        ASSERT_TRUE(pair.writer->queue_write(&ch, 1));
    }
    ASSERT_GT(pair.set.poll(2000), 0);
    EXPECT_EQ(pair.writer->queued_bytes(), 0u);
    std::string received(expected.size(), '\0');
    ASSERT_EQ(pair.reader.read_until(received.data(), 1024), 1024);
    EXPECT_EQ(received, expected);
}

TEST(WriteQueue, SingleDispatchIsBudgetedForFairness) {
    write_pair pair;
    ASSERT_TRUE(pair.start());
    const auto bytes = binary(256 * 1024);
    ASSERT_TRUE(pair.writer->set_max_queued_bytes(bytes.size()));
    ASSERT_TRUE(pair.writer->queue_write(bytes));
    ASSERT_GT(pair.set.poll(2000), 0);
    EXPECT_GE(pair.writer->queued_bytes(), bytes.size() - 64u * 1024u);
    EXPECT_LT(pair.writer->queued_bytes(), bytes.size());
}

TEST(WriteQueue, DrainHandlerCanQueueNextMessage) {
    write_pair pair;
    ASSERT_TRUE(pair.start());
    int drains = 0;
    pair.writer->on_drain([&] {
        ++drains;
        if (drains == 1) EXPECT_TRUE(pair.writer->queue_write("b", 1));
    });
    ASSERT_TRUE(pair.writer->queue_write("a", 1));
    ASSERT_GT(pair.set.poll(2000), 0);
    EXPECT_EQ(drains, 1);
    EXPECT_EQ(pair.writer->queued_bytes(), 1u);
    ASSERT_GT(pair.set.poll(2000), 0);
    EXPECT_EQ(drains, 2);
    EXPECT_EQ(pair.writer->queued_bytes(), 0u);
    char chars[2]{};
    ASSERT_EQ(pair.reader.read_until(chars, 2), 2);
    EXPECT_EQ(std::string(chars, 2), "ab");
}

TEST(WriteQueue, LocalDrainRemovalSuppressesSetCallback) {
    write_pair pair;
    ASSERT_TRUE(pair.start());
    int local = 0, global = 0;
    pair.writer->on_drain([&] {
        ++local;
        EXPECT_TRUE(pair.set.remove_socket(pair.writer));
    });
    pair.set.on_socket_drain([&](aeon::event_socket*) { ++global; });
    ASSERT_TRUE(pair.writer->queue_write("x", 1));
    ASSERT_GT(pair.set.poll(2000), 0);
    EXPECT_EQ(local, 1);
    EXPECT_EQ(global, 0);
    EXPECT_EQ(pair.set.size(), 0);
}

TEST(WriteQueue, DeregisteredSocketIsNotFlushed) {
    write_pair pair;
    ASSERT_TRUE(pair.start());
    ASSERT_TRUE(pair.writer->queue_write("x", 1));
    ASSERT_TRUE(pair.set.remove_socket(pair.writer));
    EXPECT_EQ(pair.set.poll(0), 0);
    EXPECT_EQ(pair.writer->queued_bytes(), 1u);
    ASSERT_TRUE(pair.set.add(pair.writer));
    ASSERT_GT(pair.set.poll(2000), 0);
    EXPECT_EQ(pair.writer->queued_bytes(), 0u);
    char ch{};
    ASSERT_EQ(pair.reader.read_until(&ch, 1), 1);
    EXPECT_EQ(ch, 'x');
}

TEST(WriteQueue, CloseDiscardsPendingWithoutFiringDrain) {
    write_pair pair;
    ASSERT_TRUE(pair.start());
    int drains = 0;
    pair.writer->on_drain([&] { ++drains; });
    ASSERT_TRUE(pair.writer->queue_write("unsent", 6));
    ASSERT_TRUE(pair.writer->close());
    EXPECT_EQ(pair.writer->queued_bytes(), 0u);
    EXPECT_EQ(drains, 0);
    EXPECT_FALSE(pair.writer->queue_write("x", 1));
}

TEST(WriteQueue, MultiplePollsPreserveBinaryOrderThroughBudgetedFlush) {
    write_pair pair;
    ASSERT_TRUE(pair.start());
    const auto bytes = binary(192 * 1024);
    ASSERT_TRUE(pair.writer->set_max_queued_bytes(bytes.size()));
    ASSERT_TRUE(pair.writer->queue_write(bytes));
    std::string received(bytes.size(), '\0');
    int read_count = -1;
    std::thread receiver([&] {
        read_count = pair.reader.read_until(received.data(), static_cast<int>(received.size()));
    });
    bool poll_failed = false;
    for (int attempts = 0; pair.writer->queued_bytes() && attempts < 20; ++attempts) {
        if (pair.set.poll(2000) < 0) {
            poll_failed = true;
            break;
        }
    }
    receiver.join();
    EXPECT_FALSE(poll_failed);
    EXPECT_EQ(pair.writer->queued_bytes(), 0u);
    EXPECT_EQ(read_count, static_cast<int>(received.size()));
    EXPECT_EQ(received, bytes);
}

TEST(WriteQueue, BufferedOutputDoesNotPretendToBeDirectWriteCompletion) {
    write_pair pair;
    ASSERT_TRUE(pair.start());
    int direct = 0, drained = 0;
    pair.writer->on_write([&](const char*, int, int) { ++direct; });
    pair.writer->on_drain([&] { ++drained; });
    ASSERT_TRUE(pair.writer->queue_write("queued", 6));
    ASSERT_GT(pair.set.poll(2000), 0);
    EXPECT_EQ(pair.writer->queued_bytes(), 0u);
    EXPECT_EQ(direct, 0);
    EXPECT_EQ(drained, 1);
    char data[6]{};
    EXPECT_EQ(pair.reader.read_until(data, 6), 6);
    EXPECT_EQ(std::string(data, 6), "queued");
}
