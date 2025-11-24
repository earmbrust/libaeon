/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#pragma once

#include "socket.h"

namespace net {

    class udp_socket : public socket {
    public:
        udp_socket();

        int write(char* data, int size);
        int write(const char* data, int size);
        int write(char* data);
        int write(const char* data);
        int write(const std::string& data);

        int read(char* buffer, int size);
        std::string read(int size);
        int read();
    };

} // namespace net
