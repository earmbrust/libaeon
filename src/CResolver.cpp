#ifndef _CRESOLVER_CPP
#define _CRESOLVER_CPP

#include "libaeon.h"
#include <cstring>
#include <cstdio>
#include <stdexcept>

namespace net {

/**
 * CResolver constructor
 */
CResolver::CResolver() {
}

/**
 * CResolver destructor
 */
CResolver::~CResolver() {
}

/**
 * Internal DNS resolution using getaddrinfo
 * Returns CAddress with the first available address (IPv6 preferred, then IPv4)
 */
CAddress CResolver::ResolveInternal(const char* hostname) {
    struct addrinfo hints;
    struct addrinfo* server_info = nullptr;
    struct addrinfo* p = nullptr;
    CAddress ipv6_addr;
    CAddress ipv4_addr;
    bool has_ipv6 = false;
    bool has_ipv4 = false;

    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;        // Allow both IPv4 and IPv6
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int rv = getaddrinfo(hostname, nullptr, &hints, &server_info);
    if (rv != 0) {
        std::fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(rv));
        throw std::runtime_error("Failed to resolve hostname");
    }

    // Iterate through results, preferring IPv6 but keeping IPv4 as fallback
    for (p = server_info; p != nullptr; p = p->ai_next) {
        if (p->ai_family == AF_INET6 && !has_ipv6) {
            ipv6_addr = CAddress(*(sockaddr_in6*)p->ai_addr);
            has_ipv6 = true;
        } else if (p->ai_family == AF_INET && !has_ipv4) {
            ipv4_addr = CAddress(*(sockaddr_in*)p->ai_addr);
            has_ipv4 = true;
        }
    }

    freeaddrinfo(server_info);

    // Return IPv6 if available, otherwise IPv4
    if (has_ipv6) {
        return ipv6_addr;
    } else if (has_ipv4) {
        return ipv4_addr;
    } else {
        throw std::runtime_error("No addresses found for hostname");
    }
}

/**
 * Template specialization for std::string
 * Returns the IP address as a string
 */
template<>
std::string CResolver::Resolve<std::string>(const char* hostname) {
    CAddress addr = ResolveInternal(hostname);
    return addr.GetString();
}

/**
 * Template specialization for CAddress
 * Returns the CAddress object
 */
template<>
CAddress CResolver::Resolve<CAddress>(const char* hostname) {
    return ResolveInternal(hostname);
}

/**
 * Template specialization for sockaddr_storage
 * Returns the raw sockaddr_storage
 */
template<>
sockaddr_storage CResolver::Resolve<sockaddr_storage>(const char* hostname) {
    CAddress addr = ResolveInternal(hostname);
    return addr.GetSockaddrStorage();
}

/**
 * Template specialization for sockaddr_in (IPv4)
 * Returns IPv4 address, throws if only IPv6 available
 */
template<>
sockaddr_in CResolver::Resolve<sockaddr_in>(const char* hostname) {
    CAddress addr = ResolveInternal(hostname);
    return addr.GetSockaddrIPv4();
}

/**
 * Template specialization for sockaddr_in6 (IPv6)
 * Returns IPv6 address, throws if only IPv4 available
 */
template<>
sockaddr_in6 CResolver::Resolve<sockaddr_in6>(const char* hostname) {
    CAddress addr = ResolveInternal(hostname);
    return addr.GetSockaddrIPv6();
}

} // namespace net

#endif // _CRESOLVER_CPP
