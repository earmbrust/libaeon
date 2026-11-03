/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#pragma once

#include "socket.hpp"

namespace aeon {

    class LIBAEON_API client_socket : public socket {
    public:
        client_socket();
        explicit client_socket(const char* hostname, int port);
        explicit client_socket(const std::string* hostname, int port);

        bool connect(const char* hostname, int port);
        bool connect();
        bool connect(const address& addr);

        virtual ~client_socket();
    };

} // aeon
