#include "aeon.hpp"
#include "../example/pong_server.hpp"
#include <gtest/gtest.h>
#include <algorithm>
#include <string>
#include <vector>

namespace {
struct peer {
    aeon::udp_client_event_socket socket;
    aeon::event_socket_set poller;
    std::vector<std::string> packets;

    explicit peer(int port) {
        EXPECT_TRUE(socket.connect("127.0.0.1", port));
        EXPECT_TRUE(poller.add(&socket));
        poller.on_socket_datagram([this](aeon::udp_event_socket*, const char* bytes,
                                         int count, const aeon::address&) {
            packets.emplace_back(bytes, static_cast<std::size_t>(count));
        });
    }
    void send(const char* command) {
        ASSERT_EQ(socket.write(command, static_cast<int>(std::strlen(command))),
                  static_cast<int>(std::strlen(command)));
    }
    bool has(const std::string& frame) const {
        return std::find(packets.begin(), packets.end(), frame) != packets.end();
    }
    int snapshots() const {
        return static_cast<int>(std::count_if(packets.begin(), packets.end(),
            [](const auto& p) { return p.starts_with("S "); }));
    }
};

void pump(aeon_examples::pong_server& game, std::initializer_list<peer*> receivers,
          int attempts = 16) {
    for (int i = 0; i < attempts; ++i) {
        EXPECT_GE(game.poll(4), 0);
        for (auto* p : receivers) EXPECT_GE(p->poller.poll(0), 0);
    }
}
} // namespace

TEST(UdpPong, StartAndStop) {
    aeon_examples::pong_server server;
    EXPECT_TRUE(server.start(0));
    EXPECT_GT(server.local_port(), 0);
    EXPECT_EQ(server.player_count(), 0);
    EXPECT_FALSE(server.start(0));
    server.stop();
    EXPECT_EQ(server.local_port(), -1);
    EXPECT_TRUE(server.start(0));
    server.stop();
}
TEST(UdpPong, TwoPlayersJoinAndReceiveSnapshots) {
    aeon_examples::pong_server game;
    ASSERT_TRUE(game.start(0));
    peer left(game.local_port()), right(game.local_port());
    left.send("J"); right.send("J");
    pump(game, {&left, &right});
    EXPECT_EQ(game.player_count(), 2);
    EXPECT_TRUE(left.has("W 1"));
    EXPECT_TRUE(right.has("W 2"));
    EXPECT_GT(left.snapshots(), 0);
    EXPECT_GT(right.snapshots(), 0);
    EXPECT_EQ(game.current().occupied, 3);
}
TEST(UdpPong, DuplicateJoinUsesExistingSeat) {
    aeon_examples::pong_server game;
    ASSERT_TRUE(game.start(0));
    peer left(game.local_port());
    left.send("J"); left.send("J");
    pump(game, {&left});
    EXPECT_EQ(game.player_count(), 1);
    EXPECT_EQ(game.current().occupied, 1);
    EXPECT_TRUE(left.has("W 1"));
}
TEST(UdpPong, ThirdPlayerIsRejected) {
    aeon_examples::pong_server game;
    ASSERT_TRUE(game.start(0));
    peer a(game.local_port()), b(game.local_port()), c(game.local_port());
    a.send("J"); b.send("J"); c.send("J");
    pump(game, {&a, &b, &c});
    EXPECT_EQ(game.player_count(), 2);
    EXPECT_TRUE(c.has("FULL"));
}
TEST(UdpPong, OnlyTheOwningEndpointCanMovePaddle) {
    aeon_examples::pong_server game;
    ASSERT_TRUE(game.start(0));
    peer a(game.local_port()), b(game.local_port()), intruder(game.local_port());
    a.send("J"); b.send("J");
    pump(game, {&a, &b});
    intruder.send("U");
    pump(game, {&intruder});
    game.advance(100);
    EXPECT_EQ(game.current().left_y, 4);
    a.send("U");
    pump(game, {&a, &b});
    game.advance(100);
    EXPECT_EQ(game.current().left_y, 3);
    EXPECT_EQ(game.current().right_y, 4);
}
TEST(UdpPong, UnknownAndOversizePacketsAreIgnored) {
    aeon_examples::pong_server game;
    ASSERT_TRUE(game.start(0));
    peer a(game.local_port());
    a.send("garbage");
    a.send("JJ");
    a.send("JJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJ");
    pump(game, {&a});
    EXPECT_EQ(game.player_count(), 0);
}
TEST(UdpPong, MovementStopsAndClampsAtBorders) {
    aeon_examples::pong_server game;
    ASSERT_TRUE(game.start(0));
    peer a(game.local_port()), b(game.local_port());
    a.send("J"); b.send("J");
    pump(game, {&a, &b});
    a.send("U");
    pump(game, {&a, &b});
    game.advance(400);
    game.advance(400);
    EXPECT_EQ(game.current().left_y, 0);
    a.send("N");
    pump(game, {&a, &b});
    game.advance(400);
    EXPECT_EQ(game.current().left_y, 0);
    b.send("D");
    pump(game, {&a, &b});
    game.advance(400);
    game.advance(400);
    game.advance(400);
    EXPECT_EQ(game.current().right_y, 9);
}
TEST(UdpPong, LeaveFreesSeatForAnotherEndpoint) {
    aeon_examples::pong_server game;
    ASSERT_TRUE(game.start(0));
    peer a(game.local_port()), b(game.local_port()), c(game.local_port());
    a.send("J"); b.send("J"); pump(game, {&a, &b});
    a.send("Q"); pump(game, {&a, &b});
    EXPECT_EQ(game.player_count(), 1);
    c.send("J"); pump(game, {&b, &c});
    EXPECT_TRUE(c.has("W 1"));
    EXPECT_EQ(game.player_count(), 2);
}
TEST(UdpPong, IdleTimeoutFreesPlayers) {
    aeon_examples::pong_server game;
    ASSERT_TRUE(game.start(0));
    peer a(game.local_port()), b(game.local_port());
    a.send("J"); b.send("J"); pump(game, {&a, &b});
    for (int i = 0; i < 13; ++i) game.advance(400);
    EXPECT_EQ(game.player_count(), 0);
    EXPECT_EQ(game.current().occupied, 0);
}
TEST(UdpPong, HeartbeatRenewsSeat) {
    aeon_examples::pong_server game;
    ASSERT_TRUE(game.start(0));
    peer a(game.local_port());
    a.send("J"); pump(game, {&a});
    for (int i = 0; i < 12; ++i) {
        game.advance(400);
        a.send("H"); pump(game, {&a}, 2);
    }
    EXPECT_EQ(game.player_count(), 1);
}
TEST(UdpPong, TickUpdatesSnapshotSequenceAndScoresStayValid) {
    aeon_examples::pong_server game;
    ASSERT_TRUE(game.start(0));
    peer a(game.local_port()), b(game.local_port());
    a.send("J"); b.send("J"); pump(game, {&a, &b});
    const auto before = game.current();
    game.advance(100);
    pump(game, {&a, &b});
    const auto after = game.current();
    EXPECT_GT(after.sequence, before.sequence);
    EXPECT_NE(after.ball_x, before.ball_x);
    EXPECT_GT(a.snapshots(), 0);
    EXPECT_GT(b.snapshots(), 0);
    EXPECT_GE(after.left_score, 0);
    EXPECT_GE(after.right_score, 0);
}
TEST(UdpPong, RemainsOperationalAfterManyDatagrams) {
    aeon_examples::pong_server game;
    ASSERT_TRUE(game.start(0));
    peer a(game.local_port()), b(game.local_port());
    a.send("J"); b.send("J"); pump(game, {&a, &b});
    for (int i = 0; i < 120; ++i) {
        a.send(i % 2 ? "U" : "D");
        b.send(i % 2 ? "D" : "U");
        pump(game, {&a, &b}, 2);
        game.advance(100);
    }
    EXPECT_EQ(game.player_count(), 2); // Regular input counts as activity.
    EXPECT_GT(game.current().sequence, 120U);
}

TEST(UdpPong, ScoresWhenBallPassesUnmovedPaddle) {
    aeon_examples::pong_server game;
    ASSERT_TRUE(game.start(0));
    peer a(game.local_port()), b(game.local_port());
    a.send("J"); b.send("J"); pump(game, {&a, &b});
    for (int i = 0; i < 36; ++i) game.advance(100);
    const auto state = game.current();
    EXPECT_GT(state.left_score + state.right_score, 0);
    EXPECT_EQ(game.player_count(), 2);
}
