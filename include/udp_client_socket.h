/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#pragma once

#include "udp_socket.h"

namespace net {

    class LIBAEON_API udp_client_socket : public udp_socket {
    public:
        udp_client_socket();
        udp_client_socket(const char* hostname, int port);
        explicit udp_client_socket(const std::string* hostname, int port);

        bool connect(const char* hostname, int port);
        bool connect();
        bool connect(const address& addr);

        virtual ~udp_client_socket();
    };

} // namespace net