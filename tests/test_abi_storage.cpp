#include <gtest/gtest.h>
#include <aeon.hpp>

#include <limits>
#include <string>
#include <type_traits>

namespace {
class hostname_probe final : public aeon::socket {
public:
    using aeon::socket::remote_hostname;
    using aeon::socket::set_remote_hostname;
};
} // namespace

static_assert(std::is_copy_constructible_v<aeon::socket_set>);
static_assert(std::is_copy_assignable_v<aeon::socket_set>);

TEST(AbiStorage, HostnameStartsEmptyAndIsRetainedWhenSocketCloses) {
    hostname_probe s;
    EXPECT_TRUE(s.remote_hostname().empty());
    s.set_remote_hostname("localhost");
    EXPECT_EQ(s.remote_hostname(), "localhost");
    s.close();
    EXPECT_EQ(s.remote_hostname(), "localhost");
}

TEST(AbiStorage, HostnameReplacementIsIndependentAcrossSockets) {
    hostname_probe a;
    hostname_probe b;
    const std::string large(2000, 'q');
    a.set_remote_hostname(large);
    b.set_remote_hostname("127.0.0.1");
    EXPECT_EQ(a.remote_hostname(), large);
    EXPECT_EQ(b.remote_hostname(), "127.0.0.1");
    a.set_remote_hostname("::1");
    EXPECT_EQ(a.remote_hostname(), "::1");
    EXPECT_EQ(b.remote_hostname(), "127.0.0.1");
}

TEST(AbiStorage, SocketSetRetainsNonOwningPointerSemantics) {
    aeon::socket a;
    aeon::socket b;
    aeon::socket_set set;
    EXPECT_EQ(set.size(), 0);
    EXPECT_FALSE(set.add(nullptr));
    EXPECT_TRUE(set.add(&a));
    EXPECT_TRUE(set.add(&b));
    EXPECT_EQ(set.size(), 2);
    EXPECT_TRUE(set.remove(0));
    EXPECT_EQ(set.size(), 1);
    EXPECT_TRUE(set.remove(0));
    EXPECT_EQ(set.size(), 0);
    EXPECT_TRUE(a.is_valid_socket());
    EXPECT_TRUE(b.is_valid_socket());
}

TEST(AbiStorage, InvalidRemovalsLeaveRegistryUnchanged) {
    aeon::socket a;
    aeon::socket_set set;
    ASSERT_TRUE(set.add(&a));
    EXPECT_FALSE(set.remove(1));
    EXPECT_FALSE(set.remove(0, 0));
    EXPECT_FALSE(set.remove(1, 1));
    EXPECT_EQ(set.size(), 1);
    EXPECT_TRUE(set.remove(0));
    EXPECT_FALSE(set.remove(0));
}

TEST(AbiStorage, BulkRemovalHandlesOversizedCountWithoutOverflow) {
    aeon::socket a;
    aeon::socket b;
    aeon::socket c;
    aeon::socket_set set;
    ASSERT_TRUE(set.add(&a));
    ASSERT_TRUE(set.add(&b));
    ASSERT_TRUE(set.add(&c));
    EXPECT_TRUE(set.remove(1, std::numeric_limits<unsigned int>::max()));
    EXPECT_EQ(set.size(), 1);
    EXPECT_TRUE(set.remove(0, 1));
    EXPECT_EQ(set.size(), 0);
}

TEST(AbiStorage, SocketSetPreservesExistingDuplicateRegistrationBehavior) {
    aeon::socket s;
    aeon::socket_set set;
    EXPECT_TRUE(set.add(&s));
    EXPECT_TRUE(set.add(&s));
    EXPECT_EQ(set.size(), 2);
    EXPECT_TRUE(set.remove(0, 2));
    EXPECT_EQ(set.size(), 0);
    EXPECT_TRUE(s.is_valid_socket());
}

TEST(AbiStorage, CopiesHaveIndependentNonOwningRegistries) {
    aeon::socket a;
    aeon::socket b;
    aeon::socket_set original;
    ASSERT_TRUE(original.add(&a));
    aeon::socket_set copied(original);
    EXPECT_EQ(original.size(), 1);
    EXPECT_EQ(copied.size(), 1);
    EXPECT_TRUE(copied.add(&b));
    EXPECT_EQ(original.size(), 1);
    EXPECT_EQ(copied.size(), 2);
    original = copied;
    EXPECT_EQ(original.size(), 2);
    EXPECT_TRUE(copied.remove(0, 2));
    EXPECT_EQ(original.size(), 2);
    original = original;
    EXPECT_EQ(original.size(), 2);
    EXPECT_TRUE(a.is_valid_socket());
    EXPECT_TRUE(b.is_valid_socket());
}
