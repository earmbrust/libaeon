
#include <net.h>
#include <cstring>
#include <cstdio>
#include <stdexcept>

namespace net {

/**
 * resolver constructor
 */
resolver::resolver() {
}

/**
 * resolver destructor
 */
resolver::~resolver() {
}

/**
 * Internal DNS resolution using getaddrinfo
 * Returns address with the first available address (IPv6 preferred, then IPv4)
 */
address resolver::resolve_internal(const char* hostname) {
    struct addrinfo hints;
    struct addrinfo* server_info = nullptr;
    struct addrinfo* p = nullptr;
    address ipv6_addr;
    address ipv4_addr;
    bool has_ipv6 = false;
    bool has_ipv4 = false;

    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int rv = getaddrinfo(hostname, nullptr, &hints, &server_info);
    if (rv != 0) {
        std::fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(rv));
        throw std::runtime_error("Failed to resolve hostname");
    }

    for (p = server_info; p != nullptr; p = p->ai_next) {
        if (p->ai_family == AF_INET6 && !has_ipv6) {
            ipv6_addr = address(*(sockaddr_in6*)p->ai_addr);
            has_ipv6 = true;
        } else if (p->ai_family == AF_INET && !has_ipv4) {
            ipv4_addr = address(*(sockaddr_in*)p->ai_addr);
            has_ipv4 = true;
        }
    }

    freeaddrinfo(server_info);

    if (has_ipv6) {
        return ipv6_addr;
    } else if (has_ipv4) {
        return ipv4_addr;
    } else {
        throw std::runtime_error("No addresses found for hostname");
    }
}

/**
 * Template implementation
 */
template<typename T>
T resolver::resolve(const char* hostname) {
    address addr = resolve_internal(hostname);
    
    if constexpr (std::is_same_v<T, std::string>) {
        return addr.get_string();
    } else if constexpr (std::is_same_v<T, address>) {
        return addr;
    } else if constexpr (std::is_same_v<T, sockaddr_storage>) {
        return addr.get_sockaddr_storage();
    } else if constexpr (std::is_same_v<T, sockaddr_in>) {
        return addr.get_sockaddr_ipv4();
    } else if constexpr (std::is_same_v<T, sockaddr_in6>) {
        return addr.get_sockaddr_ipv6();
    }
}

/**
 * Convenience method - resolve to string
 */
std::string resolver::resolve_to_string(const char* hostname) {
    return resolve<std::string>(hostname);
}

/**
 * Convenience method - resolve to address
 */
address resolver::resolve_to_address(const char* hostname) {
    return resolve<address>(hostname);
}

// Explicit template instantiations
template std::string resolver::resolve<std::string>(const char*);
template address resolver::resolve<address>(const char*);
template sockaddr_storage resolver::resolve<sockaddr_storage>(const char*);
template sockaddr_in resolver::resolve<sockaddr_in>(const char*);
template sockaddr_in6 resolver::resolve<sockaddr_in6>(const char*);

} // namespace net

