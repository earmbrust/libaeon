/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#pragma once

#include "client_socket.h"
#include "event_socket.h"

namespace net {

    class LIBAEON_API client_event_socket : public client_socket, public event_socket {
    public:
        client_event_socket();
        explicit client_event_socket(const char* hostname, int port);
        virtual ~client_event_socket();

        bool connect();
        bool connect(const char* hostname, int port);
        bool close();

    protected:
        virtual void on_data_impl(const char* buffer, int size) override;
        virtual void on_connect_impl() override;
        virtual void on_close_impl() override;
        virtual void on_error_impl(int error_code) override;
    };

} // namespace net
