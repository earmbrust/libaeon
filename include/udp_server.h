/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#pragma once

#include "udp_socket.h"

namespace net {

    class udp_server : public udp_socket {
    public:
        udp_server();
        virtual ~udp_server();

        bool listen(int port);
        bool listen();
    };

} // namespace net
