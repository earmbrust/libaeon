/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#pragma once

#include "server_socket.hpp"
#include "event_socket.hpp"

namespace aeon {

    class LIBAEON_API server_event_socket : public server_socket, public event_socket {
    public:
        server_event_socket();
        explicit server_event_socket(int port);
        virtual ~server_event_socket();

        bool listen(int port);
        bool listen(const char* address, int port);
        bool close();

    protected:
        virtual void on_data_impl(const char* buffer, int size) override;
        virtual void on_listen_impl() override;
        virtual void on_close_impl() override;
        virtual void on_error_impl(int error_code) override;
    };

} // aeon
