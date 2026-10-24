#include "aeon.hpp"
#include <gtest/gtest.h>
#include "event_socket.hpp"
#include "event_socket_set.hpp"
#include "udp_event_socket.hpp"
#include <stdexcept>

namespace {
class lifecycle_tcp : public aeon::event_socket {
public:
    void emit_data() { fire_on_data("test", 4); }
    void emit_error(int code) { fire_on_error(code); }
};
class lifecycle_udp : public aeon::udp_event_socket {
public:
    void emit_datagram() { aeon::address from; fire_on_datagram("a", 1, from); }
};
class lifecycle_set : public aeon::event_socket_set {
public:
    void emit_data(aeon::event_socket* socket) { fire_on_socket_data(socket, "a", 1); }
};
} // namespace

TEST(EventLifecycle, TcpDataHandlerCanCloseItself) {
    lifecycle_tcp socket;
    int close_count = 0;
    socket.on_close([&] { ++close_count; EXPECT_FALSE(socket.close()); });
    socket.on_data([&](const char*, int) { EXPECT_TRUE(socket.close()); });
    socket.emit_data();
    EXPECT_EQ(close_count, 1);
    EXPECT_FALSE(socket.is_valid_socket());
}

TEST(EventLifecycle, UdpHandlerCanCloseItself) {
    lifecycle_udp socket;
    int close_count = 0;
    socket.on_close([&] { ++close_count; EXPECT_FALSE(socket.close()); });
    socket.on_datagram([&](const char*, int, const aeon::address&) {
        EXPECT_TRUE(socket.close());
    });
    socket.emit_datagram();
    EXPECT_EQ(close_count, 1);
    EXPECT_FALSE(socket.is_valid_socket());
}

TEST(EventLifecycle, HandlerCanUnregisterSocketAndRegisterAnother) {
    lifecycle_set set;
    lifecycle_tcp first, second;
    ASSERT_TRUE(set.add(&first));
    int hits = 0;
    set.on_socket_data([&](aeon::event_socket* socket, const char*, int) {
        ++hits;
        EXPECT_EQ(socket, &first);
        EXPECT_TRUE(set.remove_socket(&first));
        EXPECT_TRUE(set.add(&second));
    });
    set.emit_data(&first);
    EXPECT_EQ(hits, 1);
    EXPECT_EQ(set.size(), 1);
    EXPECT_TRUE(first.is_valid_socket());
    EXPECT_TRUE(second.is_valid_socket());
    EXPECT_TRUE(set.remove_socket(&second));
}

TEST(EventLifecycle, CloseHandlerCanReplaceItselfWithoutReentering) {
    lifecycle_tcp socket;
    int old_hits = 0, new_hits = 0;
    socket.on_close([&] {
        ++old_hits;
        socket.on_close([&] { ++new_hits; });
        EXPECT_FALSE(socket.close());
    });
    EXPECT_TRUE(socket.close());
    EXPECT_EQ(old_hits, 1);
    EXPECT_EQ(new_hits, 0);
}

TEST(EventLifecycle, ThrowingCloseHandlerLeavesDescriptorClosed) {
    lifecycle_tcp socket;
    int fired = 0;
    socket.on_close([&] {
        ++fired;
        throw std::runtime_error("close handler");
    });
    EXPECT_THROW(socket.close(), std::runtime_error);
    EXPECT_FALSE(socket.is_valid_socket());
    EXPECT_FALSE(socket.close());
    EXPECT_EQ(fired, 1);
}

TEST(EventLifecycle, CloseTriggeredDuringErrorDispatchIsSafe) {
    lifecycle_tcp socket;
    int close_count = 0, error_count = 0;
    socket.on_close([&] { ++close_count; });
    socket.on_error([&](int code) {
        EXPECT_EQ(code, 55);
        ++error_count;
        EXPECT_TRUE(socket.close());
    });
    socket.emit_error(55);
    EXPECT_EQ(close_count, 1);
    EXPECT_EQ(error_count, 1);
}
