#include <gtest/gtest.h>

#include "aeon.hpp"

#include <cstring>

namespace {

int local_port(const aeon::socket& owner) {
    sockaddr_storage endpoint{};
    socklen_t length = sizeof(endpoint);
    if (::getsockname(owner.sockfd, reinterpret_cast<sockaddr*>(&endpoint), &length) != 0)
        return -1;
    if (endpoint.ss_family == AF_INET)
        return ntohs(reinterpret_cast<const sockaddr_in*>(&endpoint)->sin_port);
    if (endpoint.ss_family == AF_INET6)
        return ntohs(reinterpret_cast<const sockaddr_in6*>(&endpoint)->sin6_port);
    return -1;
}

bool is_open(aeon::socket_t descriptor) {
    int type = 0;
    socklen_t length = sizeof(type);
    return ::getsockopt(descriptor, SOL_SOCKET, SO_TYPE,
                        reinterpret_cast<char*>(&type), &length) == 0;
}

#if LIBAEON_HAS_EVENTS
using udp_bind_base = aeon::udp_server_event_socket;
#else
using udp_bind_base = aeon::udp_server_socket;
#endif
class udp_bind_test_server : public udp_bind_base {
public:
    using aeon::udp_socket::udp_listen;
};

struct bind_probe {
    static inline int calls = 0;
    static inline int ipv6_calls = 0;
    static inline int ipv4_calls = 0;
    static inline aeon::socket_t old_descriptor = aeon::invalid_socket;
    static inline bool passed_live_descriptor = false;

    static void reset(aeon::socket_t current) {
        calls = 0;
        ipv6_calls = 0;
        ipv4_calls = 0;
        old_descriptor = current;
        passed_live_descriptor = false;
    }

    static int denied(aeon::socket_t candidate, const sockaddr* endpoint, socklen_t) {
        ++calls;
        if (endpoint->sa_family == AF_INET6) ++ipv6_calls;
        if (endpoint->sa_family == AF_INET) ++ipv4_calls;
        if (candidate == old_descriptor) passed_live_descriptor = true;
#ifdef NET_PLATFORM_WINDOWS
        ::WSASetLastError(WSAEADDRINUSE);
#else
        errno = EADDRINUSE;
#endif
        return -1;
    }

    static int deny_ipv6(aeon::socket_t candidate, const sockaddr* endpoint,
                         socklen_t length) {
        ++calls;
        if (endpoint->sa_family == AF_INET6) {
            ++ipv6_calls;
#ifdef NET_PLATFORM_WINDOWS
            ::WSASetLastError(WSAEADDRINUSE);
#else
            errno = EADDRINUSE;
#endif
            return -1;
        }
        ++ipv4_calls;
        return ::bind(candidate, endpoint, length);
    }
};

} // namespace

TEST(UdpInternal, FailedNativeBindPreservesPreviouslyBoundSocket) {
    udp_bind_test_server server;
    ASSERT_TRUE(server.listen(0));
    const auto old_fd = server.sockfd;
    const int old_port = local_port(server);
    ASSERT_GT(old_port, 0);
    bind_probe::reset(old_fd);

    // Bind failures are injected at Aeon's actual candidate-socket boundary,
    // avoiding OS-specific port-reuse and dual-stack collision semantics.
    EXPECT_FALSE(server.udp_listen(0, &bind_probe::denied));
    EXPECT_GE(bind_probe::calls, 1);
    EXPECT_FALSE(bind_probe::passed_live_descriptor);
    EXPECT_EQ(bind_probe::calls, bind_probe::ipv6_calls + bind_probe::ipv4_calls);
    EXPECT_EQ(server.sockfd, old_fd);
    EXPECT_EQ(local_port(server), old_port);
    EXPECT_TRUE(is_open(old_fd));
#ifdef NET_PLATFORM_WINDOWS
    EXPECT_EQ(server.get_error(), WSAEADDRINUSE);
#else
    EXPECT_EQ(server.get_error(), EADDRINUSE);
#endif
    EXPECT_EQ(server.get_state(), aeon::state_bind);

    aeon::udp_client_socket client;
    ASSERT_TRUE(client.connect("127.0.0.1", old_port));
    ASSERT_EQ(server.set_read_timeout(250), 0);
    ASSERT_EQ(client.write("x", 1), 1);
    char data[8]{};
    ASSERT_EQ(server.read(data, sizeof(data)), 1);
    EXPECT_EQ(data[0], 'x');
}

TEST(UdpInternal, FailedIPv6BindFallsBackToIPv4) {
    udp_bind_test_server server;
    ASSERT_TRUE(server.listen(0));
    const auto original = server.sockfd;
    bind_probe::reset(original);

    ASSERT_TRUE(server.udp_listen(0, &bind_probe::deny_ipv6));
    EXPECT_TRUE(is_open(server.sockfd));
    EXPECT_NE(server.sockfd, original);
    EXPECT_FALSE(is_open(original));
    EXPECT_GE(bind_probe::ipv4_calls, 1);
    EXPECT_EQ(bind_probe::calls, bind_probe::ipv4_calls + bind_probe::ipv6_calls);
    EXPECT_GT(local_port(server), 0);
    EXPECT_EQ(server.get_error(), aeon::err_none);
}

TEST(UdpInternal, NullBindFunctionPreservesLiveDescriptor) {
    udp_bind_test_server server;
    ASSERT_TRUE(server.listen(0));
    const auto original = server.sockfd;
    const int port = local_port(server);
    EXPECT_FALSE(server.udp_listen(0, nullptr));
    EXPECT_EQ(server.sockfd, original);
    EXPECT_EQ(local_port(server), port);
    EXPECT_TRUE(is_open(original));
    EXPECT_EQ(server.get_state(), aeon::state_bind);
}
