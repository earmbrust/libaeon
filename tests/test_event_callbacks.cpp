#include <gtest/gtest.h>
#include "aeon.hpp"

#include <atomic>
#include <stdexcept>
#include <thread>
#include <type_traits>

namespace {
class tcp_probe : public aeon::event_socket {
public:
    int fallback_data = 0;
    int fallback_error = 0;
    int fallback_close = 0;
    void emit_data(const char* p, int n) { fire_on_data(p, n); }
    void emit_connect() { fire_on_connect(); }
    void emit_listen() { fire_on_listen(); }
    void emit_error(int c) { fire_on_error(c); }
protected:
    void on_data_impl(const char*, int) override { ++fallback_data; }
    void on_error_impl(int) override { ++fallback_error; }
    void on_close_impl() override { ++fallback_close; }
};

class udp_probe : public aeon::udp_event_socket {
public:
    int fallback = 0;
    void emit(const char* d, int n, const aeon::address& addr) {
        fire_on_datagram(d, n, addr);
    }
protected:
    void on_datagram_impl(const char*, int, const aeon::address&) override { ++fallback; }
};
class set_probe : public aeon::event_socket_set {
public:
    int fallback = 0;
    void emit(aeon::event_socket* s, const char* d, int n) {
        fire_on_socket_data(s, d, n);
    }
    void emit_error(aeon::event_socket* s, int error) {
        fire_on_socket_error(s, error);
    }
protected:
    void on_socket_data_impl(aeon::event_socket*, const char*, int) override { ++fallback; }
};
} // namespace

TEST(EventCallbacks, PublicObjectsAreNotCopyable) {
    static_assert(!std::is_copy_constructible_v<aeon::event_socket>);
    static_assert(!std::is_copy_constructible_v<aeon::udp_event_socket>);
    static_assert(!std::is_copy_constructible_v<aeon::event_socket_set>);
}
TEST(EventCallbacks, DataFallsBackToVirtualWhenEmptyAndAfterClear) {
    tcp_probe s;
    s.emit_data("a", 1);
    EXPECT_EQ(s.fallback_data, 1);
    int count = 0;
    s.on_data([&](const char*, int n) { count += n; });
    s.emit_data("abc", 3);
    EXPECT_EQ(count, 3);
    EXPECT_EQ(s.fallback_data, 1);
    s.on_data({});
    s.emit_data("b", 1);
    EXPECT_EQ(s.fallback_data, 2);
}
TEST(EventCallbacks, ReplaceSelfDuringDispatch) {
    tcp_probe s;
    int old_hits = 0, new_hits = 0;
    s.on_data([&](const char*, int) {
        ++old_hits;
        s.on_data([&](const char*, int) { ++new_hits; });
    });
    s.emit_data("a", 1);
    s.emit_data("a", 1);
    EXPECT_EQ(old_hits, 1);
    EXPECT_EQ(new_hits, 1);
}
TEST(EventCallbacks, RemoveOwnCallbackDuringDispatch) {
    tcp_probe s;
    int hits = 0;
    s.on_connect([&] { ++hits; s.on_connect({}); });
    s.emit_connect();
    s.emit_connect();
    EXPECT_EQ(hits, 1);
}
TEST(EventCallbacks, ExceptionsDoNotLockCallbackSlot) {
    tcp_probe s;
    s.on_data([](const char*, int) { throw std::runtime_error("callback"); });
    EXPECT_THROW(s.emit_data("a", 1), std::runtime_error);
    int count = 0;
    s.on_data([&](const char*, int) { ++count; });
    s.emit_data("a", 1);
    EXPECT_EQ(count, 1);
}
TEST(EventCallbacks, ConnectListenAndErrorHaveIndependentHandlers) {
    tcp_probe s;
    int connect = 0, listen = 0, error = 0;
    s.on_connect([&] { ++connect; });
    s.on_listen([&] { ++listen; });
    s.on_error([&](int e) { error = e; });
    s.emit_connect();
    s.emit_listen();
    s.emit_error(1234);
    EXPECT_EQ(connect, 1);
    EXPECT_EQ(listen, 1);
    EXPECT_EQ(error, 1234);
    EXPECT_EQ(s.fallback_error, 0);
    s.on_error({});
    s.emit_error(99);
    EXPECT_EQ(s.fallback_error, 1);
}
TEST(EventCallbacks, CloseOnlyOnceOnExplicitClose) {
    tcp_probe s;
    int hits = 0;
    s.on_close([&] { ++hits; });
    ASSERT_TRUE(s.close());
    EXPECT_FALSE(s.close());
    EXPECT_EQ(hits, 1);
}
TEST(EventCallbacks, DispatchIsSynchronousOnCallingThread) {
    tcp_probe s;
    std::thread::id callback_thread;
    const auto current = std::this_thread::get_id();
    s.on_data([&](const char*, int) { callback_thread = std::this_thread::get_id(); });
    s.emit_data("a", 1);
    EXPECT_EQ(callback_thread, current);
}
TEST(EventCallbacks, ConcurrentRegistrationAndInvocationAreSafe) {
    tcp_probe s;
    std::atomic<int> hits{0};
    std::thread writer([&] {
        for (int i = 0; i != 1000; ++i) {
            s.on_data([&](const char*, int) { ++hits; });
            if (i % 3 == 0) s.on_data({});
        }
    });
    for (int i = 0; i != 1000; ++i) s.emit_data("a", 1);
    writer.join();
    const int previous = hits.load();
    s.on_data([&](const char*, int) { ++hits; });
    s.emit_data("a", 1);
    EXPECT_EQ(hits.load(), previous + 1);
}
TEST(EventCallbacks, UdpDatagramProvidesSenderAndFallsBack) {
    udp_probe s;
    aeon::address sender;
    int bytes = 0;
    s.emit("a", 1, sender);
    EXPECT_EQ(s.fallback, 1);
    s.on_datagram([&](const char*, int n, const aeon::address& from) {
        bytes += n;
        EXPECT_EQ(&sender, &from);
    });
    s.emit("ab", 2, sender);
    EXPECT_EQ(bytes, 2);
    s.on_datagram({});
    s.emit("c", 1, sender);
    EXPECT_EQ(s.fallback, 2);
}
TEST(EventCallbacks, UdpExplicitCloseDoesNotFireTwice) {
    udp_probe s;
    int hits = 0;
    s.on_close([&] { ++hits; });
    ASSERT_TRUE(s.close());
    EXPECT_FALSE(s.close());
    EXPECT_EQ(hits, 1);
}
TEST(EventCallbacks, NonOwningSetRejectsDuplicatesAndRemovesByPointer) {
    set_probe events;
    tcp_probe a, b;
    EXPECT_FALSE(events.add(nullptr));
    EXPECT_TRUE(events.add(&a));
    EXPECT_FALSE(events.add(&a));
    EXPECT_TRUE(events.add(&b));
    EXPECT_EQ(events.size(), 2);
    EXPECT_FALSE(events.remove(static_cast<unsigned int>(100)));
    EXPECT_TRUE(events.remove_socket(&a));
    EXPECT_FALSE(events.remove_socket(&a));
    EXPECT_EQ(events.size(), 1);
    EXPECT_TRUE(a.is_valid_socket());
    EXPECT_TRUE(events.remove(0U));
    EXPECT_TRUE(b.is_valid_socket());
    EXPECT_EQ(events.size(), 0);
}
TEST(EventCallbacks, SetCallbacksUseSnapshotAndVirtualFallback) {
    set_probe events;
    tcp_probe sock;
    events.emit(&sock, "a", 1);
    EXPECT_EQ(events.fallback, 1);
    int one = 0, two = 0;
    events.on_socket_data([&](aeon::event_socket*, const char*, int) {
        ++one;
        events.on_socket_data([&](aeon::event_socket*, const char*, int) { ++two; });
    });
    events.emit(&sock, "b", 1);
    events.emit(&sock, "c", 1);
    EXPECT_EQ(one, 1);
    EXPECT_EQ(two, 1);
    events.on_socket_data({});
    events.emit(&sock, "d", 1);
    EXPECT_EQ(events.fallback, 2);
}
TEST(EventCallbacks, SetErrorIsIndependentFromData) {
    set_probe events;
    tcp_probe sock;
    int received = 0;
    events.on_socket_error([&](aeon::event_socket* s, int c) {
        EXPECT_EQ(s, &sock);
        received = c;
    });
    events.emit_error(&sock, 123);
    EXPECT_EQ(received, 123);
}

TEST(EventCallbacks, ReplacementDoesNotDestroyExecutingHandler) {
    tcp_probe probe;
    int old = 0;
    int next = 0;
    probe.on_data([&](const char*, int) {
        ++old;
        probe.on_data([&](const char*, int) { ++next; });
        EXPECT_EQ(old, 1);
    });
    probe.emit_data("a", 1);
    probe.emit_data("b", 1);
    EXPECT_EQ(old, 1);
    EXPECT_EQ(next, 1);
}

// Legacy CEventSocket::OnWrite reported each direct send, including its
// requested length and the actual result. The new event API retains that
// information without conflating it with queued-write drain notifications.
namespace {
class write_notification_probe final : public aeon::event_socket {
public:
    bool fail_send = false;
    int virtual_writes = 0;
    int fallback_requested = 0;
    int fallback_sent = 0;
protected:
    int poll_send(const char*, int length, int) override {
        if (fail_send) {
#ifdef NET_PLATFORM_WINDOWS
            WSASetLastError(WSAECONNRESET);
#else
            errno = ECONNRESET;
#endif
            return -1;
        }
        return length > 2 ? 2 : length;
    }
    void on_write_impl(const char*, int requested, int sent) override {
        ++virtual_writes;
        fallback_requested = requested;
        fallback_sent = sent;
    }
};
}

TEST(EventCallbacks, DirectWriteReportsRequestedAndActualBytesOnce) {
    write_notification_probe s;
    ASSERT_TRUE(s.is_valid_socket());
    int count = 0, requested = -1, sent = -1;
    std::string captured;
    s.on_write([&](const char* data, int want, int got) {
        ++count; requested = want; sent = got;
        captured.assign(data, static_cast<std::size_t>(want));
    });
    char mutable_data[] = "hello";
    EXPECT_EQ(s.write(mutable_data), 2);
    EXPECT_EQ(count, 1);
    EXPECT_EQ(requested, 5);
    EXPECT_EQ(sent, 2);
    EXPECT_EQ(captured, "hello");
    const char const_data[] = {'a', '\0', 'b'};
    EXPECT_EQ(s.write(const_data, 3), 2);
    EXPECT_EQ(count, 2);
    EXPECT_EQ(requested, 3);
    EXPECT_EQ(captured, std::string(const_data, 3));
    EXPECT_EQ(s.write(std::string("A\0B", 3)), 2);
    EXPECT_EQ(count, 3);
    aeon::socket* polymorphic = &s;
    EXPECT_EQ(polymorphic->write("X", 1), 1);
    EXPECT_EQ(count, 4);
    EXPECT_EQ(s.virtual_writes, 0);
    EXPECT_EQ(s.write(""), 0);
    EXPECT_EQ(s.write("", 0), -1);
    EXPECT_EQ(count, 4);
}

TEST(EventCallbacks, DirectWriteFailureReportsZeroAndRetainsErrorNotification) {
    write_notification_probe s;
    s.fail_send = true;
    int requested = 0, sent = -1, errors = 0;
    s.on_write([&](const char*, int need, int actual) {
        requested = need; sent = actual;
    });
    s.on_error([&](int code) {
        ++errors;
        EXPECT_EQ(code, aeon::err_connection_reset);
    });
    EXPECT_EQ(s.write("abc"), -1);
    EXPECT_EQ(s.get_error(), aeon::err_connection_reset);
    EXPECT_EQ(requested, 3);
    EXPECT_EQ(sent, 0);
    EXPECT_EQ(errors, 1);
}

TEST(EventCallbacks, WriteCallbackSupportsVirtualFallbackAndReplacement) {
    write_notification_probe s;
    ASSERT_TRUE(s.is_valid_socket());
    EXPECT_EQ(s.write("abc"), 2);
    EXPECT_EQ(s.virtual_writes, 1);
    EXPECT_EQ(s.fallback_requested, 3);
    EXPECT_EQ(s.fallback_sent, 2);
    int first = 0, replacement = 0;
    s.on_write([&](const char*, int, int) {
        ++first;
        s.on_write([&](const char*, int, int) { ++replacement; });
    });
    EXPECT_EQ(s.write("abc"), 2);
    EXPECT_EQ(s.write("abc"), 2);
    EXPECT_EQ(first, 1);
    EXPECT_EQ(replacement, 1);
    s.on_write({});
    EXPECT_EQ(s.write("y", 1), 1);
    EXPECT_EQ(s.virtual_writes, 2);
}
