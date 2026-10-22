#include <gtest/gtest.h>
#include "internal/native_poller.hpp"
#include "udp_server_socket.hpp"
#include "udp_client_socket.hpp"
#include <vector>

namespace {
int port_of(const aeon::socket& owner) {
    sockaddr_storage bound{};
    socklen_t len = static_cast<socklen_t>(sizeof(bound));
    if (::getsockname(owner.sockfd, reinterpret_cast<sockaddr*>(&bound), &len) != 0) return -1;
    if (bound.ss_family == AF_INET6)
        return ntohs(reinterpret_cast<const sockaddr_in6*>(&bound)->sin6_port);
    if (bound.ss_family == AF_INET)
        return ntohs(reinterpret_cast<const sockaddr_in*>(&bound)->sin_port);
    return -1;
}
const char* host_of(const aeon::socket& owner) {
    sockaddr_storage bound{};
    socklen_t len = static_cast<socklen_t>(sizeof(bound));
    if (::getsockname(owner.sockfd, reinterpret_cast<sockaddr*>(&bound), &len) != 0) return nullptr;
    return bound.ss_family == AF_INET6 ? "::1" : bound.ss_family == AF_INET ? "127.0.0.1" : nullptr;
}
} // namespace

TEST(NativePollerInternal, EmptyAndInvalidTimeout) {
    std::vector<aeon::internal::poll_descriptor> entries;
    EXPECT_EQ(aeon::internal::poll_readable(entries, -2), -1);
    EXPECT_EQ(aeon::internal::poll_readable(entries, 0), 0);
    EXPECT_EQ(aeon::internal::poll_readable(entries, -1), 0);
}

TEST(NativePollerInternal, ValidSocketTimesOutWithoutChangingEvents) {
    aeon::udp_server_socket listener;
    ASSERT_TRUE(listener.listen(0));
    std::vector<aeon::internal::poll_descriptor> entries{{listener.sockfd, 0}};
    EXPECT_EQ(aeon::internal::poll_readable(entries, 0), 0);
    EXPECT_EQ(entries[0].revents, 0);
}

TEST(NativePollerInternal, PollReportsDatagramReadiness) {
    aeon::udp_server_socket listener;
    ASSERT_TRUE(listener.listen(0));
    aeon::udp_client_socket sender;
    const char* host = host_of(listener);
    ASSERT_NE(host, nullptr);
    ASSERT_TRUE(sender.connect(host, port_of(listener)));
    ASSERT_EQ(sender.write("ready", 5), 5);
    std::vector<aeon::internal::poll_descriptor> entries{{listener.sockfd, 0}};
    ASSERT_GT(aeon::internal::poll_readable(entries, 2000), 0);
    EXPECT_NE(entries[0].revents, 0);
}

TEST(NativePollerInternal, InvalidDescriptorAndWouldBlockAreRecognized) {
    EXPECT_NE(aeon::internal::invalid_descriptor_error(), 0);
#ifdef NET_PLATFORM_WINDOWS
    EXPECT_TRUE(aeon::internal::would_block_error(WSAEWOULDBLOCK));
#else
    EXPECT_TRUE(aeon::internal::would_block_error(EAGAIN));
    EXPECT_TRUE(aeon::internal::would_block_error(EINTR));
#endif
    EXPECT_FALSE(aeon::internal::would_block_error(0));
}

TEST(NativePollerInternal, ReusedDescriptorClearsOldReadinessOnTimeout) {
    aeon::udp_server_socket listener;
    ASSERT_TRUE(listener.listen(0));
    std::vector<aeon::internal::poll_descriptor> entries{{listener.sockfd, static_cast<short>(0x7fff)}};
    EXPECT_EQ(aeon::internal::poll_readable(entries, 0), 0);
    EXPECT_EQ(entries[0].revents, 0);
    entries[0].revents = static_cast<short>(0x7fff);
    EXPECT_EQ(aeon::internal::poll_readable(entries, 0), 0);
    EXPECT_EQ(entries[0].revents, 0);
}

TEST(NativePollerInternal, ReusedDescriptorClearsReadinessAfterDatagramConsumed) {
    aeon::udp_server_socket listener;
    ASSERT_TRUE(listener.listen(0));
    aeon::udp_client_socket sender;
    const char* host = host_of(listener);
    ASSERT_NE(host, nullptr);
    ASSERT_TRUE(sender.connect(host, port_of(listener)));
    ASSERT_EQ(sender.write("r", 1), 1);
    std::vector<aeon::internal::poll_descriptor> entries{{listener.sockfd, 0}};
    ASSERT_GT(aeon::internal::poll_readable(entries, 2000), 0);
    EXPECT_NE(entries[0].revents, 0);
    char buf[4];
    sockaddr_storage peer{};
    socklen_t length = static_cast<socklen_t>(sizeof(peer));
    ASSERT_EQ(::recvfrom(listener.sockfd, buf, sizeof(buf), 0,
        reinterpret_cast<sockaddr*>(&peer), &length), 1);
    EXPECT_EQ(aeon::internal::poll_readable(entries, 0), 0);
    EXPECT_EQ(entries[0].revents, 0);
}

TEST(NativePollerInternal, OnlyReadyDatagramSocketGetsReadinessFlags) {
    aeon::udp_server_socket a;
    aeon::udp_server_socket b;
    ASSERT_TRUE(a.listen(0));
    ASSERT_TRUE(b.listen(0));
    aeon::udp_client_socket sender;
    const char* host = host_of(b);
    ASSERT_NE(host, nullptr);
    ASSERT_TRUE(sender.connect(host, port_of(b)));
    ASSERT_EQ(sender.write("ready", 5), 5);
    std::vector<aeon::internal::poll_descriptor> entries{{a.sockfd, 0}, {b.sockfd, 0}};
    ASSERT_GT(aeon::internal::poll_readable(entries, 2000), 0);
    EXPECT_EQ(entries[0].revents, 0);
    EXPECT_NE(entries[1].revents, 0);
}
