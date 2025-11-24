/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#pragma once

#include "socket.h"

namespace net {

    class event_socket : public socket {
    public:
        bool poll();

        int write(char* data);
        int write(const char* data);
        int write(const std::string& data);

    protected:
        virtual bool on_read(const char* buffer, int size);
        virtual void on_write(const char* buffer, int size, int sentsize);
    };

} // namespace net
