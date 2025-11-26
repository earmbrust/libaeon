/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#pragma once

/**
 * \file config.hpp
 * \brief Platform detection and configuration
 * \internal This file is part of the internal implementation.
 *           Users should not include this directly.
 */

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
    #include <arpa/inet.hpp>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <sys/types.h>
    #include <unistd.h>
#elif defined(__linux__)
    #define NET_PLATFORM_LINUX
    #include <sys/socket.h>
    #include <netdb.h>
    #include <arpa/inet.hpp>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <sys/types.h>
    #include <unistd.h>
#else
    // Generic POSIX fallback
    #define NET_PLATFORM_POSIX
    #include <sys/socket.h>
    #include <netdb.h>
    #include <arpa/inet.hpp>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <sys/types.h>
    #include <unistd.h>
#endif

// Multi-platform includes
#include <fcntl.h>
#include <memory>
#include <string>
#include <vector>
#include <cstring>
#include <cstdlib>

namespace aeon {
    namespace internal {
        // Platform-native socket type abstraction
        #ifdef NET_PLATFORM_WINDOWS
            typedef SOCKET socket_t;
            constexpr socket_t invalid_socket = INVALID_SOCKET;
        #else
            typedef int socket_t;
            constexpr socket_t invalid_socket = -1;
        #endif

// Error constants
        constexpr int socket_error = -1;
        constexpr int invalid_return = -1;

        // Platform-specific error retrieval
        #ifdef NET_PLATFORM_WINDOWS
            #define GET_NET_SOCKET_ERROR() WSAGetLastError()
            #define NET_SOCKET_ERROR SOCKET_ERROR
        #else
            #include <errno.h>
            #define GET_NET_SOCKET_ERROR() errno
            #define NET_SOCKET_ERROR -1
        #endif

        // Socket state constants
        enum socket_state : int {
            state_resolve = 1,
            state_create = 2,
            state_accept = 3,
            state_connect = 4,
            state_bind = 5
        };

        // Error code constants
        enum error_code : int {
            err_none = 0,
            err_no_host = 1,
            err_no_socket = 2
        };

        // IPv4/IPv6 constants
        constexpr int socket_ipv4 = 0;
        constexpr int socket_ipv6 = 1;
        
        // Socket flags
        constexpr int null_flag = 0;

        // Utility function
        inline bool is_valid_port(int port) {
            return port >= 0 && port <= 65535;
        }

    } // namespace internal

    // Export commonly used types to public namespace
    using socket_t = internal::socket_t;
    
    // Export error and state constants as constexpr
    constexpr int err_none = 0;
    constexpr int err_no_host = 1;
    constexpr int err_no_socket = 2;
    constexpr int state_resolve = 1;
    constexpr int state_create = 2;
    constexpr int state_accept = 3;
    constexpr int state_connect = 4;
    constexpr int state_bind = 5;
    constexpr socket_t invalid_socket = internal::invalid_socket;
    constexpr int socket_error = internal::socket_error;
    constexpr int null_flag = 0;

    inline bool is_valid_port(int port) {
        return internal::is_valid_port(port);
    }

} // aeon
