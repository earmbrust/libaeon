#include <gtest/gtest.h>
#include "address.h"
#include <cstring>

using namespace net;

class AddressTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code if needed
    }
};

TEST_F(AddressTest, DefaultConstructor) {
    address addr;
    // Default address should be initialized
    EXPECT_FALSE(addr.get_string().empty());
}

TEST_F(AddressTest, IPv4AddressFromSockaddr) {
    sockaddr_in ipv4{};
    ipv4.sin_family = AF_INET;
    ipv4.sin_port = htons(8080);
    inet_pton(AF_INET, "192.168.1.1", &ipv4.sin_addr);

    address addr(ipv4);
    EXPECT_TRUE(addr.is_ipv4());
    EXPECT_FALSE(addr.is_ipv6());
    EXPECT_EQ(addr.get_port(), 8080);
    EXPECT_NE(addr.get_string().find("192.168.1.1"), std::string::npos);
}

TEST_F(AddressTest, IPv6AddressFromSockaddr) {
    sockaddr_in6 ipv6{};
    ipv6.sin6_family = AF_INET6;
    ipv6.sin6_port = htons(9090);
    inet_pton(AF_INET6, "::1", &ipv6.sin6_addr);

    address addr(ipv6);
    EXPECT_TRUE(addr.is_ipv6());
    EXPECT_FALSE(addr.is_ipv4());
    EXPECT_EQ(addr.get_port(), 9090);
    EXPECT_NE(addr.get_string().find("::1"), std::string::npos);
}

TEST_F(AddressTest, SetPortIPv4) {
    sockaddr_in ipv4{};
    ipv4.sin_family = AF_INET;
    ipv4.sin_port = htons(8080);
    inet_pton(AF_INET, "10.0.0.1", &ipv4.sin_addr);

    address addr(ipv4);
    EXPECT_EQ(addr.get_port(), 8080);

    addr.set_port(3000);
    EXPECT_EQ(addr.get_port(), 3000);
}

TEST_F(AddressTest, SetPortIPv6) {
    sockaddr_in6 ipv6{};
    ipv6.sin6_family = AF_INET6;
    ipv6.sin6_port = htons(9090);
    inet_pton(AF_INET6, "2001:db8::1", &ipv6.sin6_addr);

    address addr(ipv6);
    EXPECT_EQ(addr.get_port(), 9090);

    addr.set_port(5000);
    EXPECT_EQ(addr.get_port(), 5000);
}

TEST_F(AddressTest, GetSockaddrStorageIPv4) {
    sockaddr_in ipv4{};
    ipv4.sin_family = AF_INET;
    ipv4.sin_port = htons(8080);
    inet_pton(AF_INET, "172.16.0.1", &ipv4.sin_addr);

    address addr(ipv4);
    sockaddr_storage storage = addr.get_sockaddr_storage();

    EXPECT_EQ(storage.ss_family, AF_INET);
    sockaddr_in* result = reinterpret_cast<sockaddr_in*>(&storage);
    EXPECT_EQ(result->sin_port, htons(8080));
}

TEST_F(AddressTest, GetSockaddrStorageIPv6) {
    sockaddr_in6 ipv6{};
    ipv6.sin6_family = AF_INET6;
    ipv6.sin6_port = htons(5000);
    inet_pton(AF_INET6, "fe80::1", &ipv6.sin6_addr);

    address addr(ipv6);
    sockaddr_storage storage = addr.get_sockaddr_storage();

    EXPECT_EQ(storage.ss_family, AF_INET6);
    sockaddr_in6* result = reinterpret_cast<sockaddr_in6*>(&storage);
    EXPECT_EQ(result->sin6_port, htons(5000));
}

TEST_F(AddressTest, GetSockaddrIPv4) {
    sockaddr_in ipv4{};
    ipv4.sin_family = AF_INET;
    ipv4.sin_port = htons(2000);
    inet_pton(AF_INET, "127.0.0.1", &ipv4.sin_addr);

    address addr(ipv4);
    sockaddr_in result = addr.get_sockaddr_ipv4();

    EXPECT_EQ(result.sin_family, AF_INET);
    EXPECT_EQ(result.sin_port, htons(2000));
}

TEST_F(AddressTest, GetSockaddrIPv6) {
    sockaddr_in6 ipv6{};
    ipv6.sin6_family = AF_INET6;
    ipv6.sin6_port = htons(4000);
    inet_pton(AF_INET6, "::ffff:192.0.2.1", &ipv6.sin6_addr);

    address addr(ipv6);
    sockaddr_in6 result = addr.get_sockaddr_ipv6();

    EXPECT_EQ(result.sin6_family, AF_INET6);
    EXPECT_EQ(result.sin6_port, htons(4000));
}

TEST_F(AddressTest, LocalhostIPv4) {
    sockaddr_in ipv4{};
    ipv4.sin_family = AF_INET;
    ipv4.sin_port = htons(8000);
    inet_pton(AF_INET, "127.0.0.1", &ipv4.sin_addr);

    address addr(ipv4);
    EXPECT_TRUE(addr.is_ipv4());
    EXPECT_NE(addr.get_string().find("127.0.0.1"), std::string::npos);
}

TEST_F(AddressTest, LocalhostIPv6) {
    sockaddr_in6 ipv6{};
    ipv6.sin6_family = AF_INET6;
    ipv6.sin6_port = htons(8000);
    inet_pton(AF_INET6, "::1", &ipv6.sin6_addr);

    address addr(ipv6);
    EXPECT_TRUE(addr.is_ipv6());
    EXPECT_NE(addr.get_string().find("::1"), std::string::npos);
}

TEST_F(AddressTest, PortBoundaryValues) {
    sockaddr_in ipv4{};
    ipv4.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &ipv4.sin_addr);

    address addr(ipv4);

    // Test minimum port
    addr.set_port(1);
    EXPECT_EQ(addr.get_port(), 1);

    // Test maximum port
    addr.set_port(65535);
    EXPECT_EQ(addr.get_port(), 65535);

    // Test common ports
    addr.set_port(80);
    EXPECT_EQ(addr.get_port(), 80);

    addr.set_port(443);
    EXPECT_EQ(addr.get_port(), 443);
}

TEST_F(AddressTest, SockaddrStorageFromIPv4ThenIPv6) {
    // Create IPv4 address
    sockaddr_in ipv4{};
    ipv4.sin_family = AF_INET;
    ipv4.sin_port = htons(8080);
    inet_pton(AF_INET, "192.168.1.100", &ipv4.sin_addr);

    address addr(ipv4);
    EXPECT_TRUE(addr.is_ipv4());

    // Replace with IPv6
    sockaddr_in6 ipv6{};
    ipv6.sin6_family = AF_INET6;
    ipv6.sin6_port = htons(9000);
    inet_pton(AF_INET6, "2001:db8:85a3::8a2e:370:7334", &ipv6.sin6_addr);

    address addr2(ipv6);
    EXPECT_TRUE(addr2.is_ipv6());
    EXPECT_FALSE(addr2.is_ipv4());
}

TEST_F(AddressTest, PrivateIPv4Ranges) {
    // Test 10.0.0.0/8 range
    sockaddr_in ipv4_10{};
    ipv4_10.sin_family = AF_INET;
    ipv4_10.sin_port = htons(1234);
    inet_pton(AF_INET, "10.255.255.255", &ipv4_10.sin_addr);

    address addr10(ipv4_10);
    EXPECT_TRUE(addr10.is_ipv4());
    EXPECT_NE(addr10.get_string().find("10.255.255.255"), std::string::npos);

    // Test 172.16.0.0/12 range
    sockaddr_in ipv4_172{};
    ipv4_172.sin_family = AF_INET;
    ipv4_172.sin_port = htons(5678);
    inet_pton(AF_INET, "172.31.255.255", &ipv4_172.sin_addr);

    address addr172(ipv4_172);
    EXPECT_TRUE(addr172.is_ipv4());
    EXPECT_NE(addr172.get_string().find("172.31.255.255"), std::string::npos);

    // Test 192.168.0.0/16 range
    sockaddr_in ipv4_192{};
    ipv4_192.sin_family = AF_INET;
    ipv4_192.sin_port = htons(9012);
    inet_pton(AF_INET, "192.168.255.255", &ipv4_192.sin_addr);

    address addr192(ipv4_192);
    EXPECT_TRUE(addr192.is_ipv4());
    EXPECT_NE(addr192.get_string().find("192.168.255.255"), std::string::npos);
}
