#ifndef _CADDRESS_CPP
#define _CADDRESS_CPP

#include "libaeon.h"
#include <cstring>
#include <stdexcept>

namespace net {

/**
 * Default constructor - creates empty address
 */
CAddress::CAddress() : family(0) {
    std::memset(&addr, 0, sizeof(addr));
}

/**
 * Constructor from sockaddr_storage
 */
CAddress::CAddress(const sockaddr_storage& src_addr) : addr(src_addr) {
    family = src_addr.ss_family;
}

/**
 * Constructor from sockaddr_in (IPv4)
 */
CAddress::CAddress(const sockaddr_in& src_addr) {
    family = AF_INET;
    std::memset(&addr, 0, sizeof(addr));
    std::memcpy(&addr, &src_addr, sizeof(sockaddr_in));
}

/**
 * Constructor from sockaddr_in6 (IPv6)
 */
CAddress::CAddress(const sockaddr_in6& src_addr) {
    family = AF_INET6;
    std::memset(&addr, 0, sizeof(addr));
    std::memcpy(&addr, &src_addr, sizeof(sockaddr_in6));
}

bool CAddress::IsIPv4() const {
    return family == AF_INET;
}

bool CAddress::IsIPv6() const {
    return family == AF_INET6;
}

/**
 * Get address as string representation
 * Returns "a.b.c.d" for IPv4 or "x:x:x:x:x:x:x:x" for IPv6
 */
std::string CAddress::GetString() const {
    char buffer[INET6_ADDRSTRLEN];
    
    if (family == AF_INET) {
        sockaddr_in* addr4 = (sockaddr_in*)&addr;
        inet_ntop(AF_INET, &addr4->sin_addr, buffer, sizeof(buffer));
    } else if (family == AF_INET6) {
        sockaddr_in6* addr6 = (sockaddr_in6*)&addr;
        inet_ntop(AF_INET6, &addr6->sin6_addr, buffer, sizeof(buffer));
    } else {
        return "";
    }
    
    return std::string(buffer);
}

/**
 * Get port number
 * Works for both IPv4 and IPv6
 */
uint16_t CAddress::GetPort() const {
    if (family == AF_INET) {
        sockaddr_in* addr4 = (sockaddr_in*)&addr;
        return ntohs(addr4->sin_port);
    } else if (family == AF_INET6) {
        sockaddr_in6* addr6 = (sockaddr_in6*)&addr;
        return ntohs(addr6->sin6_port);
    }
    return 0;
}

/**
 * Set port number
 * Works for both IPv4 and IPv6
 */
void CAddress::SetPort(uint16_t port) {
    uint16_t port_nbo = htons(port);
    
    if (family == AF_INET) {
        sockaddr_in* addr4 = (sockaddr_in*)&addr;
        addr4->sin_port = port_nbo;
    } else if (family == AF_INET6) {
        sockaddr_in6* addr6 = (sockaddr_in6*)&addr;
        addr6->sin6_port = port_nbo;
    }
}

sockaddr_storage CAddress::GetSockaddrStorage() const {
    return addr;
}

/**
 * Get as sockaddr_in (IPv4)
 * Throws if this is not an IPv4 address
 */
sockaddr_in CAddress::GetSockaddrIPv4() const {
    if (family != AF_INET) {
        throw std::runtime_error("CAddress is not IPv4");
    }
    return *(sockaddr_in*)&addr;
}

/**
 * Get as sockaddr_in6 (IPv6)
 * Throws if this is not an IPv6 address
 */
sockaddr_in6 CAddress::GetSockaddrIPv6() const {
    if (family != AF_INET6) {
        throw std::runtime_error("CAddress is not IPv6");
    }
    return *(sockaddr_in6*)&addr;
}

} // namespace net

#endif // _CADDRESS_CPP
