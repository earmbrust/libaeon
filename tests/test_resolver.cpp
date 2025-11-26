#include <gtest/gtest.h>
#include "resolver.h"

using namespace net;

class ResolverTest : public ::testing::Test {
};

TEST_F(ResolverTest, ResolverConstruction) {
    resolver res;
}

TEST_F(ResolverTest, ResolveLocalhostToAddress) {
    resolver res;
    address addr = res.resolve_to_address("localhost");
    EXPECT_TRUE(addr.is_ipv4() || addr.is_ipv6());
}

TEST_F(ResolverTest, ResolveIPv4AddressToAddress) {
    resolver res;
    address addr = res.resolve_to_address("127.0.0.1");
    EXPECT_TRUE(addr.is_ipv4());
}

TEST_F(ResolverTest, ResolveIPv6AddressToAddress) {
    resolver res;
    address addr = res.resolve_to_address("::1");
    EXPECT_TRUE(addr.is_ipv6());
}

TEST_F(ResolverTest, ResolvePrivateIPv4) {
    resolver res;
    address addr = res.resolve_to_address("192.168.1.1");
    EXPECT_TRUE(addr.is_ipv4());
}

TEST_F(ResolverTest, ResolveToStringBasic) {
    resolver res;
    std::string result = res.resolve_to_string("127.0.0.1");
    EXPECT_FALSE(result.empty());
}

TEST_F(ResolverTest, ResolveToStringLocalhost) {
    resolver res;
    std::string result = res.resolve_to_string("localhost");
    EXPECT_FALSE(result.empty());
}

TEST_F(ResolverTest, ResolveAnyIPv4) {
    resolver res;
    address addr = res.resolve_to_address("0.0.0.0");
    EXPECT_TRUE(addr.is_ipv4());
}

TEST_F(ResolverTest, ResolveBroadcastIPv4) {
    resolver res;
    address addr = res.resolve_to_address("255.255.255.255");
    EXPECT_TRUE(addr.is_ipv4());
}

TEST_F(ResolverTest, ResolveWildcardIPv6) {
    resolver res;
    address addr = res.resolve_to_address("::");
    EXPECT_TRUE(addr.is_ipv6());
}

TEST_F(ResolverTest, ResolvePrivateIPv4RangeA) {
    resolver res;
    address addr = res.resolve_to_address("10.0.0.1");
    EXPECT_TRUE(addr.is_ipv4());
}

TEST_F(ResolverTest, ResolvePrivateIPv4RangeB) {
    resolver res;
    address addr = res.resolve_to_address("172.16.0.1");
    EXPECT_TRUE(addr.is_ipv4());
}

TEST_F(ResolverTest, ResolvePrivateIPv4RangeC) {
    resolver res;
    address addr = res.resolve_to_address("192.168.0.1");
    EXPECT_TRUE(addr.is_ipv4());
}

TEST_F(ResolverTest, ResolveLinkLocalIPv6) {
    resolver res;
    address addr = res.resolve_to_address("fe80::1");
    EXPECT_TRUE(addr.is_ipv6());
}

TEST_F(ResolverTest, ResolveULAIPv6) {
    resolver res;
    address addr = res.resolve_to_address("fd00::1");
    EXPECT_TRUE(addr.is_ipv6());
}

TEST_F(ResolverTest, ResolveIPv6Full) {
    resolver res;
    address addr = res.resolve_to_address("2001:0db8:85a3:0000:0000:8a2e:0370:7334");
    EXPECT_TRUE(addr.is_ipv6());
}

TEST_F(ResolverTest, ResolveIPv6Abbreviated) {
    resolver res;
    address addr = res.resolve_to_address("2001:db8::8a2e:370:7334");
    EXPECT_TRUE(addr.is_ipv6());
}

TEST_F(ResolverTest, ResolveSequential) {
    resolver res;
    
    address addr1 = res.resolve_to_address("127.0.0.1");
    EXPECT_TRUE(addr1.is_ipv4());
    
    address addr2 = res.resolve_to_address("192.168.1.1");
    EXPECT_TRUE(addr2.is_ipv4());
    
    address addr3 = res.resolve_to_address("::1");
    EXPECT_TRUE(addr3.is_ipv6());
}

TEST_F(ResolverTest, ResolveLoopbackIPv4) {
    resolver res;
    address addr = res.resolve_to_address("127.0.0.1");
    EXPECT_TRUE(addr.is_ipv4());
    std::string str = addr.get_string();
    EXPECT_FALSE(str.empty());
}

TEST_F(ResolverTest, ResolveLoopbackIPv6) {
    resolver res;
    address addr = res.resolve_to_address("::1");
    EXPECT_TRUE(addr.is_ipv6());
    std::string str = addr.get_string();
    EXPECT_FALSE(str.empty());
}

TEST_F(ResolverTest, ResolveAddressStringRep) {
    resolver res;
    address addr = res.resolve_to_address("192.168.1.100");
    std::string str = addr.get_string();
    EXPECT_FALSE(str.empty());
}

TEST_F(ResolverTest, ResolveAddressIPv4Check) {
    resolver res;
    address addr = res.resolve_to_address("10.20.30.40");
    EXPECT_TRUE(addr.is_ipv4());
    EXPECT_FALSE(addr.is_ipv6());
}

TEST_F(ResolverTest, ResolveAddressIPv6Check) {
    resolver res;
    address addr = res.resolve_to_address("2001:db8::1");
    EXPECT_TRUE(addr.is_ipv6());
    EXPECT_FALSE(addr.is_ipv4());
}

TEST_F(ResolverTest, ResolveToAddressIPv4) {
    resolver res;
    address addr = res.resolve_to_address("172.31.255.255");
    EXPECT_TRUE(addr.is_ipv4());
}

TEST_F(ResolverTest, ResolveToAddressIPv6) {
    resolver res;
    address addr = res.resolve_to_address("fe80::1");
    EXPECT_TRUE(addr.is_ipv6());
}

TEST_F(ResolverTest, ResolveToStringIPv4) {
    resolver res;
    std::string result = res.resolve_to_string("10.10.10.10");
    EXPECT_FALSE(result.empty());
}

TEST_F(ResolverTest, ResolveToStringIPv6) {
    resolver res;
    std::string result = res.resolve_to_string("::1");
    EXPECT_FALSE(result.empty());
}

TEST_F(ResolverTest, ResolveMaxIPv4) {
    resolver res;
    address addr = res.resolve_to_address("255.255.255.255");
    EXPECT_TRUE(addr.is_ipv4());
}

TEST_F(ResolverTest, ResolveMinIPv4) {
    resolver res;
    address addr = res.resolve_to_address("0.0.0.0");
    EXPECT_TRUE(addr.is_ipv4());
}