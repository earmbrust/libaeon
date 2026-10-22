#include <gtest/gtest.h>
#include "chat_server.hpp"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <memory>
#include <string>
#include <vector>

namespace {
struct chat_client {
    aeon::client_socket socket;
    std::string received;
    bool eof = false;

    bool connect(int port) {
        return socket.connect("127.0.0.1", port) && socket.set_blocking(false) == 0;
    }
    bool send(const std::string& text) {
        // The integration messages fit within a normal loopback send buffer.
        return socket.write(text) == static_cast<int>(text.size());
    }
    void drain() {
        char bytes[2048];
        for (int i = 0; i != 32; ++i) {
            const int n = static_cast<int>(::recv(socket.sockfd, bytes, sizeof(bytes), 0));
            if (n > 0) {
                received.append(bytes, static_cast<std::size_t>(n));
            } else {
                if (n == 0) eof = true;
                // Windows may report an accepted-but-immediately-rejected
                // connection as reset rather than clean EOF.
                if (n < 0) {
#ifdef NET_PLATFORM_WINDOWS
                    const int error = ::WSAGetLastError();
                    if (error == WSAECONNRESET || error == WSAECONNABORTED) eof = true;
#else
                    if (errno == ECONNRESET || errno == ECONNABORTED) eof = true;
#endif
                }
                break;
            }
        }
    }
};

template<class Predicate>
bool pump_until(aeon_examples::chat_server& server,
                const std::vector<chat_client*>& clients,
                Predicate predicate, int limit_ms = 3500) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(limit_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        if (server.poll(5) < 0) return false;
        for (auto* client : clients) client->drain();
        if (predicate()) return true;
    }
    return predicate();
}

bool has(const chat_client& client, const std::string& text) {
    return client.received.find(text) != std::string::npos;
}
} // namespace

TEST(ChatServer, StartsOnEphemeralPortAndStopsIdempotently) {
    aeon_examples::chat_server server;
    EXPECT_EQ(server.local_port(), -1);
    EXPECT_EQ(server.poll(0), -1);
    ASSERT_TRUE(server.start("127.0.0.1", 0));
    EXPECT_TRUE(server.running());
    EXPECT_GT(server.local_port(), 0);
    EXPECT_EQ(server.client_count(), 0u);
    EXPECT_FALSE(server.start("127.0.0.1", 0));
    server.stop();
    server.stop();
    EXPECT_FALSE(server.running());
    EXPECT_EQ(server.local_port(), -1);
    EXPECT_TRUE(server.start("127.0.0.1", 0));
}

TEST(ChatServer, GreetsEachClientAndBroadcastsToSenderAndPeers) {
    aeon_examples::chat_server server;
    ASSERT_TRUE(server.start("127.0.0.1", 0));
    chat_client first, second;
    ASSERT_TRUE(first.connect(server.local_port()));
    ASSERT_TRUE(pump_until(server, {&first}, [&] {
        return has(first, "* connected as guest-1\n");
    }));
    ASSERT_TRUE(second.connect(server.local_port()));
    ASSERT_TRUE(pump_until(server, {&first, &second}, [&] {
        return has(second, "* connected as guest-2\n");
    }));
    ASSERT_TRUE(first.send("hello\n"));
    ASSERT_TRUE(pump_until(server, {&first, &second}, [&] {
        return has(first, "guest-1: hello\n") && has(second, "guest-1: hello\n");
    }));
    ASSERT_TRUE(second.send("reply\n"));
    ASSERT_TRUE(pump_until(server, {&first, &second}, [&] {
        return has(first, "guest-2: reply\n") && has(second, "guest-2: reply\n");
    }));
}

TEST(ChatServer, FragmentsAndCoalescedLinesRetainBoundariesAndOrder) {
    aeon_examples::chat_server server;
    ASSERT_TRUE(server.start("127.0.0.1", 0));
    chat_client first, second;
    ASSERT_TRUE(first.connect(server.local_port()));
    ASSERT_TRUE(second.connect(server.local_port()));
    ASSERT_TRUE(pump_until(server, {&first, &second}, [&] { return server.client_count() == 2; }));
    ASSERT_TRUE(first.send("par"));
    for (int i = 0; i < 3; ++i) ASSERT_GE(server.poll(0), 0);
    EXPECT_FALSE(has(second, "guest-1: par"));
    ASSERT_TRUE(first.send("tial\r\nnext\nthird\n"));
    ASSERT_TRUE(pump_until(server, {&first, &second}, [&] {
        return has(second, "guest-1: third\n");
    }));
    const auto a = second.received.find("guest-1: partial\n");
    const auto b = second.received.find("guest-1: next\n");
    const auto c = second.received.find("guest-1: third\n");
    EXPECT_NE(a, std::string::npos);
    EXPECT_NE(b, std::string::npos);
    EXPECT_NE(c, std::string::npos);
    EXPECT_LT(a, b);
    EXPECT_LT(b, c);
}

TEST(ChatServer, PartialLinesAreIsolatedPerClient) {
    aeon_examples::chat_server server;
    ASSERT_TRUE(server.start("127.0.0.1", 0));
    chat_client first, second;
    ASSERT_TRUE(first.connect(server.local_port()));
    ASSERT_TRUE(second.connect(server.local_port()));
    ASSERT_TRUE(pump_until(server, {&first, &second}, [&] { return server.client_count() == 2; }));
    ASSERT_TRUE(first.send("alpha"));
    ASSERT_TRUE(second.send("beta\n"));
    ASSERT_TRUE(pump_until(server, {&first, &second}, [&] {
        return has(first, "guest-2: beta\n");
    }));
    EXPECT_FALSE(has(second, "guest-1: alpha"));
    ASSERT_TRUE(first.send("\n"));
    ASSERT_TRUE(pump_until(server, {&first, &second}, [&] {
        return has(second, "guest-1: alpha\n");
    }));
}

TEST(ChatServer, OverlongInputDisconnectsOnlyOffender) {
    aeon_examples::chat_limits limits;
    limits.max_line_bytes = 12;
    aeon_examples::chat_server server(limits);
    ASSERT_TRUE(server.start("127.0.0.1", 0));
    chat_client bad, good;
    ASSERT_TRUE(bad.connect(server.local_port()));
    ASSERT_TRUE(good.connect(server.local_port()));
    ASSERT_TRUE(pump_until(server, {&bad, &good}, [&] { return server.client_count() == 2; }));
    ASSERT_TRUE(bad.send("this line is too long\n"));
    ASSERT_TRUE(pump_until(server, {&bad, &good}, [&] { return server.client_count() == 1; }));
    ASSERT_TRUE(good.send("ok\n"));
    ASSERT_TRUE(pump_until(server, {&bad, &good}, [&] { return has(good, "guest-2: ok\n"); }));
    EXPECT_FALSE(has(good, "guest-1: this"));
}

TEST(ChatServer, NullByteInputDisconnectsOffender) {
    aeon_examples::chat_server server;
    ASSERT_TRUE(server.start("127.0.0.1", 0));
    chat_client first;
    ASSERT_TRUE(first.connect(server.local_port()));
    ASSERT_TRUE(pump_until(server, {&first}, [&] { return server.client_count() == 1; }));
    ASSERT_TRUE(first.send(std::string("x\0y\n", 4)));
    ASSERT_TRUE(pump_until(server, {&first}, [&] { return server.client_count() == 0; }));
}

TEST(ChatServer, PeerEofDoesNotStopRemainingClients) {
    aeon_examples::chat_server server;
    ASSERT_TRUE(server.start("127.0.0.1", 0));
    chat_client first, second;
    ASSERT_TRUE(first.connect(server.local_port()));
    ASSERT_TRUE(second.connect(server.local_port()));
    ASSERT_TRUE(pump_until(server, {&first, &second}, [&] { return server.client_count() == 2; }));
    first.socket.close();
    ASSERT_TRUE(pump_until(server, {&second}, [&] { return server.client_count() == 1; }));
    ASSERT_TRUE(second.send("still here\n"));
    ASSERT_TRUE(pump_until(server, {&second}, [&] { return has(second, "guest-2: still here\n"); }));
    server.stop();
    EXPECT_EQ(server.client_count(), 0u);
    EXPECT_FALSE(server.running());
}

TEST(ChatServer, ClientCapacityDoesNotDisplaceExistingSessions) {
    aeon_examples::chat_limits limits;
    limits.max_clients = 2;
    aeon_examples::chat_server server(limits);
    ASSERT_TRUE(server.start("127.0.0.1", 0));
    chat_client first, second, third;
    ASSERT_TRUE(first.connect(server.local_port()));
    ASSERT_TRUE(second.connect(server.local_port()));
    ASSERT_TRUE(pump_until(server, {&first, &second}, [&] { return server.client_count() == 2; }));
    ASSERT_TRUE(third.connect(server.local_port()));
    ASSERT_TRUE(pump_until(server, {&first, &second, &third}, [&] { return third.eof; }));
    EXPECT_EQ(server.client_count(), 2u);
    ASSERT_TRUE(first.send("full\n"));
    ASSERT_TRUE(pump_until(server, {&first, &second}, [&] { return has(second, "guest-1: full\n"); }));
}

TEST(ChatServer, QueuePressureCanDisconnectAClientWithoutStallingPoller) {
    aeon_examples::chat_limits limits;
    limits.max_queued_bytes = 48;
    aeon_examples::chat_server server(limits);
    ASSERT_TRUE(server.start("127.0.0.1", 0));
    chat_client first;
    ASSERT_TRUE(first.connect(server.local_port()));
    ASSERT_TRUE(pump_until(server, {&first}, [&] { return has(first, "* connected as guest-1\n"); }));
    ASSERT_TRUE(first.send("abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz\n"));
    ASSERT_TRUE(pump_until(server, {&first}, [&] { return server.client_count() == 0; }));
    EXPECT_GE(server.poll(0), 0);
}

TEST(ChatServer, ManyConcurrentClientsReceiveSameBroadcast) {
    constexpr std::size_t clients_count = 105;
    aeon_examples::chat_server server;
    ASSERT_TRUE(server.start("127.0.0.1", 0));
    std::vector<std::unique_ptr<chat_client>> owners;
    std::vector<chat_client*> clients;
    owners.reserve(clients_count);
    clients.reserve(clients_count);
    for (std::size_t i = 0; i < clients_count; ++i) {
        auto client = std::make_unique<chat_client>();
        ASSERT_TRUE(client->connect(server.local_port()));
        clients.push_back(client.get());
        owners.push_back(std::move(client));
        if ((i % 16) == 15) ASSERT_GE(server.poll(0), 0);
    }
    ASSERT_TRUE(pump_until(server, clients, [&] {
        return server.client_count() == clients_count;
    }, 5000));
    ASSERT_TRUE(clients.front()->send("broadcast\n"));
    ASSERT_TRUE(pump_until(server, clients, [&] {
        return std::all_of(clients.begin(), clients.end(), [](const auto* client) {
            return has(*client, "guest-1: broadcast\n");
        });
    }, 6000));
    EXPECT_EQ(server.client_count(), clients_count);
    server.stop();
    EXPECT_EQ(server.client_count(), 0u);
}
