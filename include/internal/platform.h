/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#pragma once

#include "config.h"

/**
 * \file platform.h
 * \brief Platform-specific abstractions and macros
 * \internal This file is part of the internal implementation.
 *           Users should not include this directly.
 */

namespace net {
    namespace internal {

        // Cross-platform socket close
        #ifdef NET_PLATFORM_WINDOWS
            #define NET_CLOSE_SOCKET(s) closesocket(s)
        #else
            #define NET_CLOSE_SOCKET(s) close(s)
        #endif

        // Cross-platform get socket error
        #ifdef NET_PLATFORM_WINDOWS
            #define NET_GET_SOCKET_ERROR() WSAGetLastError()
        #else
            #include <errno.h>
            #define NET_GET_SOCKET_ERROR() errno
        #endif

    } // namespace internal
} // namespace net
