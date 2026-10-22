#include <gtest/gtest.h>
#include "internal/callback_state.hpp"

namespace {
TEST(CallbackStateInternal, EmptySnapshotHasNoHandler) {
    aeon::internal::callback_slot<void(int)> callback;
    EXPECT_FALSE(static_cast<bool>(callback.snapshot()));
}
TEST(CallbackStateInternal, SnapshotRetainsOriginalHandlerAfterReplacement) {
    aeon::internal::callback_slot<void(int)> callback;
    int first = 0, second = 0;
    callback.set([&](int n) { first += n; });
    auto captured = callback.snapshot();
    ASSERT_TRUE(static_cast<bool>(captured));
    callback.set([&](int n) { second += n; });
    (*captured)(2);
    auto current = callback.snapshot();
    (*current)(3);
    EXPECT_EQ(first, 2);
    EXPECT_EQ(second, 3);
}
TEST(CallbackStateInternal, ClearingReleasesActiveRegistrationButNotSnapshot) {
    aeon::internal::callback_slot<void()> callback;
    int invoked = 0;
    callback.set([&] { ++invoked; });
    auto pending = callback.snapshot();
    callback.set({});
    EXPECT_FALSE(static_cast<bool>(callback.snapshot()));
    (*pending)();
    EXPECT_EQ(invoked, 1);
}
TEST(CallbackStateInternal, OldHandlerCanClearNewHandlerInsideDispatch) {
    aeon::internal::callback_slot<void()> callback;
    int hits = 0;
    callback.set([&] { ++hits; callback.set({}); });
    auto pending = callback.snapshot();
    (*pending)();
    EXPECT_EQ(hits, 1);
    EXPECT_FALSE(static_cast<bool>(callback.snapshot()));
}
} // namespace
