/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#pragma once

#include "socket.hpp"

namespace aeon {

    class LIBAEON_API udp_socket : public socket {
    public:
        udp_socket();

        int write(char* data, int size);
        int write(const char* data, int size);
        int write(char* data);
        int write(const char* data);
        int write(const std::string& data);

        // Read one datagram into caller-owned storage. size is the full
        // capacity; payload bytes are not NUL-terminated.
        int read(char* buffer, int size);
        // Binary-safe datagram. Empty data also represents an empty datagram,
        // timeout, or error; inspect n/get_error() to distinguish them.
        std::string read(int size);
        // Legacy internal buffer read (up to 255 data bytes).
        int read();
    };

} // aeon
