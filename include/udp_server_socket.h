/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#pragma once

#include "udp_socket.h"

namespace net {

    class LIBAEON_API udp_server_socket : public udp_socket {
    public:
        udp_server_socket();
        virtual ~udp_server_socket();
        bool listen(int port);
        bool listen();
    private:
        bool listening;
    };

} // namespace net