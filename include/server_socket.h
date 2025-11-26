/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#pragma once

#include "socket.h"
#include <memory>

namespace net {

    class event_socket;

    class LIBAEON_API server_socket : public socket {
    public:
        server_socket();
        virtual ~server_socket();

        int set_accept_timeout(int timeout_ms);
        int bind(const sockaddr* addr, socklen_t addrlen);
        bool listen();
        bool listen(int port);
        bool listen(const char* address, int port);

        std::unique_ptr<event_socket> accept();
        std::unique_ptr<event_socket> accept(bool blocking);
        event_socket* accept(event_socket* client_socket, bool blocking);

    private:
        socket_t server_socket_;
        int accept_timeout_ms;
    };

} // namespace net
