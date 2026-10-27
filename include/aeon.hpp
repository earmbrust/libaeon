/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#pragma once

// CMake targets provide feature switches. Manual consumers default to the
// standard build profile unless they select the options used by their library.
#ifndef LIBAEON_HAS_EVENTS
#define LIBAEON_HAS_EVENTS 1
#endif

#ifndef LIBAEON_HAS_SSL
#ifdef ENABLE_SSL
#define LIBAEON_HAS_SSL 1
#else
#define LIBAEON_HAS_SSL 0
#endif
#endif

#if LIBAEON_HAS_SSL && !defined(ENABLE_SSL)
#define ENABLE_SSL 1
#endif

// Export shared-library symbols; static consumers disable DLL imports.
#if defined(LIBAEON_STATIC)
#define LIBAEON_API
#elif defined(_WIN32) || defined(_WIN64)
#if defined(LIBAEON_BUILDING_DLL)
#define LIBAEON_API __declspec(dllexport)
#else
#define LIBAEON_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) && (__GNUC__ >= 4)
#define LIBAEON_API __attribute__((visibility("default")))
#else
#define LIBAEON_API
#endif

// Platform detection
#if defined(_WIN32) || defined(_WIN64)
    #define NOMINMAX
    #define NET_PLATFORM_WINDOWS
    #define WIN32_LEAN_AND_MEAN
    #define _CRT_SECURE_NO_DEPRECATE 1
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <io.h>
    typedef int socklen_t;
#elif defined(__APPLE__)
    #define NET_PLATFORM_MACOS
    #include <sys/socket.h>
    #include <netdb.h>
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <sys/types.h>
    #include <unistd.h>
#elif defined(__linux__)
    #define NET_PLATFORM_LINUX
    #include <sys/socket.h>
    #include <netdb.h>
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <sys/types.h>
    #include <unistd.h>
#else
    // Generic POSIX fallback
    #define NET_PLATFORM_POSIX
    #include <sys/socket.h>
    #include <netdb.h>
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <sys/types.h>
    #include <unistd.h>
#endif

// Native socket descriptor cleanup.
#ifdef NET_PLATFORM_WINDOWS
    #define NET_CLOSE_SOCKET(s) ::closesocket(s)
    #define NET_SOCKET_ERROR SOCKET_ERROR
    #define GET_NET_SOCKET_ERROR() WSAGetLastError()
#else
    #define NET_CLOSE_SOCKET(s) ::close(s)
    #define NET_SOCKET_ERROR -1
    #define GET_NET_SOCKET_ERROR() errno
#endif

// Common headers
#include <fcntl.h>
#include <memory>
#include <cstddef>
#include <string>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cerrno>

namespace aeon {
namespace internal {
#ifdef NET_PLATFORM_WINDOWS
    using socket_t = SOCKET;
    constexpr socket_t invalid_socket = INVALID_SOCKET;
#else
    using socket_t = int;
    constexpr socket_t invalid_socket = -1;
#endif

    inline int invalid_descriptor_error() noexcept {
#ifdef NET_PLATFORM_WINDOWS
        return WSAENOTSOCK;
#else
        return EBADF;
#endif
    }

    inline bool would_block_error(int error) noexcept {
#ifdef NET_PLATFORM_WINDOWS
        return error == WSAEWOULDBLOCK;
#else
        return error == EAGAIN || error == EWOULDBLOCK || error == EINTR;
#endif
    }
}

using socket_t = internal::socket_t;
constexpr socket_t invalid_socket = internal::invalid_socket;
constexpr int socket_error = -1;
constexpr int null_flag = 0;
constexpr int err_none = 0;
constexpr int err_no_host = 1;
constexpr int err_no_socket = 2;
constexpr int state_resolve = 1;
constexpr int state_create = 2;
constexpr int state_accept = 3;
constexpr int state_connect = 4;
constexpr int state_bind = 5;
inline bool is_valid_port(int port) { return port >= 0 && port <= 65535; }
} // namespace aeon

namespace aeon {
class socket;
class udp_socket;
class address;
class socket_set;

namespace internal {
class LIBAEON_API peer_collection {
protected:
    peer_collection();
    ~peer_collection();
    peer_collection(const peer_collection&) = delete;
    peer_collection& operator=(const peer_collection&) = delete;

    socket* add_peer(std::unique_ptr<socket> peer);
    bool remove_owned_peer(socket* peer);
    bool close_owned_peers();
    socket_set& peer_members();
    const socket_set& peer_members() const;
    int owned_peer_count() const;
    socket* owned_peer_at(unsigned int index) const;
    socket* find_udp_peer(const address& remote) const;
    udp_socket* get_or_create_udp_peer(udp_socket* server, const address& remote);

private:
    struct state;
    state* peers_;
};

#if LIBAEON_HAS_EVENTS
class LIBAEON_API event_handlers {
protected:
    explicit event_handlers(std::size_t count);
    ~event_handlers();
    event_handlers(const event_handlers&) = delete;
    event_handlers& operator=(const event_handlers&) = delete;

    void set_handler(std::size_t slot, std::shared_ptr<const void> handler);
    std::shared_ptr<const void> get_handler(std::size_t slot) const;

private:
    struct state;
    state* callbacks_;
};
#endif

} // namespace internal
} // namespace aeon

#include "address.hpp"
#include "socket.hpp"
#include "resolver.hpp"
#include "client_socket.hpp"
#include "server_socket.hpp"
#include "socket_set.hpp"
#include "udp_socket.hpp"
#include "udp_client_socket.hpp"
#include "udp_server_socket.hpp"

#if LIBAEON_HAS_EVENTS
#include "event_socket.hpp"
#include "client_event_socket.hpp"
#include "server_event_socket.hpp"
#include "event_socket_set.hpp"
#include "udp_event_socket.hpp"
#include "udp_client_event_socket.hpp"
#include "udp_server_event_socket.hpp"
#endif

#ifdef ENABLE_SSL
#include "ssl_socket.hpp"
#include "ssl_client_socket.hpp"
#include "ssl_server_socket.hpp"
#if LIBAEON_HAS_EVENTS
#include "ssl_event_socket.hpp"
#include "ssl_server_event_socket.hpp"
#endif
#endif

namespace aeon {

    /** \brief Return the compiled Aeon library version as a string. */
    LIBAEON_API const char* get_library_version();

} // aeon