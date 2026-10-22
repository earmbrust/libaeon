#include <gtest/gtest.h>
#include "internal/socket_registry.hpp"
#include "event_socket.hpp"

#include <atomic>
#include <thread>
#include <vector>

using aeon::internal::socket_registry;

TEST(SocketRegistryInternal, RejectsNullDuplicatesAndInvalidRemoval) {
    socket_registry registry;
    aeon::event_socket first, second;
    EXPECT_FALSE(registry.add(nullptr));
    EXPECT_TRUE(registry.add(&first));
    EXPECT_FALSE(registry.add(&first));
    EXPECT_TRUE(registry.add(&second));
    EXPECT_EQ(registry.size(), 2);
    EXPECT_FALSE(registry.remove_index(99));
    EXPECT_FALSE(registry.remove_socket(nullptr));
    EXPECT_TRUE(registry.remove_index(0));
    EXPECT_TRUE(registry.remove_socket(&second));
    EXPECT_FALSE(registry.remove_socket(&second));
    EXPECT_EQ(registry.size(), 0);
    EXPECT_TRUE(first.is_valid_socket());
    EXPECT_TRUE(second.is_valid_socket());
}

TEST(SocketRegistryInternal, RemovalInvalidatesSnapshotWithoutDestroyingSocket) {
    aeon::event_socket socket;
    socket_registry registry;
    ASSERT_TRUE(registry.add(&socket));
    const auto before = registry.snapshot();
    ASSERT_EQ(before.size(), 1U);
    EXPECT_EQ(before[0]->get_if_active(), &socket);
    ASSERT_TRUE(registry.remove_socket(&socket));
    EXPECT_EQ(before[0]->get_if_active(), nullptr);
    EXPECT_TRUE(registry.snapshot().empty());
    EXPECT_TRUE(socket.is_valid_socket());
}

TEST(SocketRegistryInternal, ReaddedSocketGetsDifferentToken) {
    aeon::event_socket socket;
    socket_registry registry;
    ASSERT_TRUE(registry.add(&socket));
    auto old = registry.snapshot().at(0);
    ASSERT_TRUE(registry.remove_socket(&socket));
    ASSERT_TRUE(registry.add(&socket));
    auto current = registry.snapshot().at(0);
    EXPECT_NE(old.get(), current.get());
    EXPECT_EQ(old->get_if_active(), nullptr);
    EXPECT_EQ(current->get_if_active(), &socket);
}

TEST(SocketRegistryInternal, RegistryDestructionInvalidatesRetainedTokens) {
    aeon::event_socket socket;
    socket_registry::token token;
    {
        socket_registry registry;
        ASSERT_TRUE(registry.add(&socket));
        token = registry.snapshot().at(0);
    }
    EXPECT_EQ(token->get_if_active(), nullptr);
    EXPECT_TRUE(socket.is_valid_socket());
}

TEST(SocketRegistryInternal, SnapshotCanBeIteratedWhileRegistryChanges) {
    aeon::event_socket first, second;
    socket_registry registry;
    ASSERT_TRUE(registry.add(&first));
    const auto pending = registry.snapshot();
    ASSERT_TRUE(registry.remove_socket(&first));
    ASSERT_TRUE(registry.add(&second));
    int dispatched = 0;
    for (const auto& entry : pending)
        if (entry->get_if_active()) ++dispatched;
    EXPECT_EQ(dispatched, 0);
    EXPECT_EQ(registry.snapshot().at(0)->get_if_active(), &second);
}

TEST(SocketRegistryInternal, ConcurrentModificationAndSnapshotAreSynchronized) {
    aeon::event_socket socket;
    socket_registry registry;
    std::atomic<bool> ready{false};
    std::thread writer([&] {
        while (!ready.load(std::memory_order_acquire)) std::this_thread::yield();
        for (int i = 0; i < 400; ++i) {
            registry.add(&socket);
            registry.remove_socket(&socket);
        }
    });
    ready.store(true, std::memory_order_release);
    for (int i = 0; i < 400; ++i) {
        auto snapshot = registry.snapshot();
        EXPECT_TRUE(snapshot.size() <= 1U);
        for (auto& entry : snapshot) {
            auto* current = entry->get_if_active();
            EXPECT_TRUE(current == nullptr || current == &socket);
        }
    }
    writer.join();
    EXPECT_EQ(registry.size(), 0);
}
