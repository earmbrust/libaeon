#include <gtest/gtest.h>
#include "internal/write_queue.hpp"
#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

namespace {
using aeon::internal::pending_write_state;
using aeon::internal::send_attempt;
using aeon::internal::flush_pending_writes;

void enqueue(pending_write_state& q, const std::string& value) {
    q.chunks.push_back(value);
    q.bytes += value.size();
}
int backpressure_code() {
#ifdef NET_PLATFORM_WINDOWS
    return WSAEWOULDBLOCK;
#else
    return EAGAIN;
#endif
}
int interrupt_code() {
#ifdef NET_PLATFORM_WINDOWS
    return WSAEINTR;
#else
    return EINTR;
#endif
}
} // namespace

TEST(WriteQueueInternal, EmptyQueueDoesNotSendOrDrain) {
    pending_write_state q;
    int calls = 0;
    const auto r = flush_pending_writes(q, 1024, [&](const char*, int) -> send_attempt {
        ++calls; return {1, 0};
    });
    EXPECT_EQ(calls, 0); EXPECT_EQ(r.bytes, 0u); EXPECT_FALSE(r.drained);
}

TEST(WriteQueueInternal, ZeroBudgetMakesNoProgress) {
    pending_write_state q; enqueue(q, "abcd");
    const auto r = flush_pending_writes(q, 0, [](const char*, int) -> send_attempt {
        return {1, 0};
    });
    EXPECT_EQ(r.bytes, 0u); EXPECT_EQ(q.bytes, 4u); EXPECT_FALSE(r.drained);
}

TEST(WriteQueueInternal, ShortWritesPreserveByteOrderAcrossChunks) {
    pending_write_state q; enqueue(q, std::string("ab\0cd", 5)); enqueue(q, "EFGH");
    std::string received;
    int calls = 0;
    const auto send = [&](const char* data, int length) -> send_attempt {
        ++calls;
        const int step = std::min(length, 2);
        received.append(data, step);
        return {step, 0};
    };
    const auto a = flush_pending_writes(q, 4, send);
    EXPECT_EQ(a.bytes, 4u); EXPECT_FALSE(a.drained); EXPECT_EQ(q.bytes, 5u);
    const auto b = flush_pending_writes(q, 64, send);
    EXPECT_EQ(b.bytes, 5u); EXPECT_TRUE(b.drained); EXPECT_EQ(q.front_offset, 0u);
    EXPECT_EQ(q.chunks.size(), 0u); EXPECT_EQ(calls, 5);
    EXPECT_EQ(received, std::string("ab\0cdEFGH", 9));
}

TEST(WriteQueueInternal, PartialProgressThenWouldBlockKeepsRemainder) {
    pending_write_state q; enqueue(q, "abcdef");
    int calls = 0;
    const auto a = flush_pending_writes(q, 1024, [&](const char*, int) -> send_attempt {
        return ++calls == 1 ? send_attempt{2, 0} : send_attempt{-1, backpressure_code()};
    });
    EXPECT_EQ(a.bytes, 2u); EXPECT_EQ(a.error, 0); EXPECT_FALSE(a.drained);
    EXPECT_EQ(q.front_offset, 2u); EXPECT_EQ(q.bytes, 4u);
    std::string received;
    const auto b = flush_pending_writes(q, 1024, [&](const char* data, int n) -> send_attempt {
        received.assign(data, n); return {n, 0};
    });
    EXPECT_TRUE(b.drained); EXPECT_EQ(received, "cdef"); EXPECT_EQ(q.bytes, 0u);
}

TEST(WriteQueueInternal, InterruptedCallRetriesAndCompletes) {
    pending_write_state q; enqueue(q, "xyz");
    int calls = 0;
    const auto r = flush_pending_writes(q, 1024, [&](const char*, int n) -> send_attempt {
        if (++calls == 1) return {-1, interrupt_code()};
        return {n, 0};
    });
    EXPECT_EQ(calls, 2); EXPECT_TRUE(r.drained); EXPECT_EQ(r.bytes, 3u);
}

TEST(WriteQueueInternal, RepeatedInterruptionsYieldWithoutSpinning) {
    pending_write_state q; enqueue(q, "xyz");
    int calls = 0;
    const auto r = flush_pending_writes(q, 1024, [&](const char*, int) -> send_attempt {
        ++calls; return {-1, interrupt_code()};
    });
    EXPECT_EQ(calls, 5); EXPECT_EQ(r.bytes, 0u); EXPECT_EQ(r.error, 0);
    EXPECT_EQ(q.bytes, 3u); EXPECT_FALSE(r.drained);
}

TEST(WriteQueueInternal, FatalErrorAfterPartialSendDiscardsRemainder) {
    pending_write_state q; enqueue(q, "abcdef"); enqueue(q, "more");
    int calls = 0;
    const auto r = flush_pending_writes(q, 1024, [&](const char*, int) -> send_attempt {
        return ++calls == 1 ? send_attempt{2, 0} : send_attempt{-1, 3456};
    });
    EXPECT_EQ(r.bytes, 2u); EXPECT_EQ(r.error, 3456); EXPECT_FALSE(r.drained);
    EXPECT_EQ(q.bytes, 0u); EXPECT_EQ(q.front_offset, 0u); EXPECT_EQ(q.chunks.size(), 0u);
}

TEST(WriteQueueInternal, ZeroByteSendStopsWithoutLooping) {
    pending_write_state q; enqueue(q, "abc");
    int calls = 0;
    const auto r = flush_pending_writes(q, 1024, [&](const char*, int) -> send_attempt {
        ++calls; return {0, 0};
    });
    EXPECT_EQ(calls, 1); EXPECT_EQ(q.bytes, 3u); EXPECT_FALSE(r.drained);
}

TEST(WriteQueueInternal, InvalidNativeResultClearsQueue) {
    pending_write_state q; enqueue(q, "abc");
    const auto r = flush_pending_writes(q, 1024, [](const char*, int n) -> send_attempt {
        return {n+1, 0};
    });
    EXPECT_NE(r.error, 0); EXPECT_FALSE(r.drained); EXPECT_EQ(q.bytes, 0u);
}

TEST(WriteQueueInternal, BudgetEnforcesPerInvocationFairness) {
    pending_write_state q; enqueue(q, std::string(256 * 1024, 'x'));
    std::size_t observed = 0;
    for (int i=0;i<4;++i) {
        const auto r = flush_pending_writes(q, 64*1024,
            [&](const char* data, int n) -> send_attempt {
                EXPECT_EQ(*data, 'x'); observed += static_cast<std::size_t>(n);
                return {n, 0};
            });
        EXPECT_EQ(r.bytes, 64u*1024u);
        EXPECT_EQ(r.drained, i == 3);
        EXPECT_EQ(q.bytes, 256u*1024u - observed);
    }
    EXPECT_EQ(observed, 256u*1024u);
}

TEST(WriteQueueInternal, ThousandsOfChunksDrainWithoutReordering) {
    pending_write_state q;
    std::string expected;
    for (int i=0;i<5000;++i) {
        const char ch = static_cast<char>(i % 256);
        enqueue(q, std::string(1, ch)); expected.push_back(ch);
    }
    std::string actual;
    for (int attempt=0; attempt<100 && q.bytes; ++attempt) {
        const auto r = flush_pending_writes(q, 79, [&](const char* data, int n) -> send_attempt {
            actual.append(data, n); return {n, 0};
        });
        EXPECT_EQ(r.error, 0);
        EXPECT_TRUE(r.bytes <= 79);
    }
    EXPECT_EQ(actual, expected); EXPECT_EQ(q.bytes, 0u);
}
