/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#ifndef _LIBAEON_COMMON_H
#define _LIBAEON_COMMON_H

// Platform detection - unified approach
#if defined(_WIN32) || defined(_WIN64)
    #define NOMINMAX
    #define PLATFORM_WINDOWS
    #define WIN32_LEAN_AND_MEAN
    #define _CRT_SECURE_NO_DEPRECATE 1
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <io.h>
    typedef int socklen_t;
#elif defined(__APPLE__)
    #define PLATFORM_MACOS
    #include <sys/socket.h>
    #include <netdb.h>
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <sys/types.h>
    #include <unistd.h>
#elif defined(__linux__)
    #define PLATFORM_LINUX
    #include <sys/socket.h>
    #include <netdb.h>
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <sys/types.h>
    #include <unistd.h>
#else
    // Generic POSIX fallback
    #include <sys/socket.h>
    #include <netdb.h>
    #include <arpa/inet.h>
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

namespace net {
    // Platform-native socket type - libaeon abstraction
    #ifdef PLATFORM_WINDOWS
        typedef SOCKET socket_t;
        #define INVALID_SOCKET_T ((net::socket_t)(INVALID_SOCKET))
    #else
        typedef int socket_t;
        #define INVALID_SOCKET_T (-1)
    #endif

    // Library-specific error constants
    #define NET_SOCKET_ERROR (-1)

    // Platform-specific socket operation macros
    #ifdef PLATFORM_WINDOWS
        #define CLOSE_SOCKET(s) closesocket(s)
        #define GET_NET_SOCKET_ERROR() WSAGetLastError()
    #else
        #define CLOSE_SOCKET(s) close(s)
        #define GET_NET_SOCKET_ERROR() errno
    #endif

    // Error and state definitions
    #define SOCK_RESOLVE 1
    #define SOCK_CREATE 2
    #define SOCK_ACCEPT 3
    #define SOCK_CONNECT 4
    #define SOCK_BIND 5
    #define ERR_NONE 0
    #define ERR_NOHOST 1
    #define ERR_NOSOCKET 2
    #define AEON 1

    // IPv4/IPv6 socket family constants
    const int SOCKET_IPV4 = 0;
    const int SOCKET_IPV6 = 1;

    /**
     * \brief Validate if a port number is in valid range
     * \param port Port number to validate
     * \return true if port is in valid range (0-65535), false otherwise
     */
    inline bool IsValidPort(int port) {
        return port >= 0 && port <= 65535;
    }
}

#endif // _LIBAEON_COMMON_H
