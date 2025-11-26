/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#include "server_event_socket.h"

namespace net {

    server_event_socket::server_event_socket()
        : server_socket(), event_socket() {
    }

    server_event_socket::server_event_socket(int port)
        : server_socket(), event_socket() {
        if (listen(port)) {
            event_socket::start_polling();
            fire_on_listen();
        }
    }

    server_event_socket::~server_event_socket() {
        event_socket::stop_polling();
    }

    bool server_event_socket::listen(int port) {
        if (!server_socket::listen(port)) {
            return false;
        }
        event_socket::start_polling();
        fire_on_listen();
        return true;
    }

    bool server_event_socket::listen(const char* address, int port) {
        if (!server_socket::listen(address, port)) {
            return false;
        }
        event_socket::start_polling();
        fire_on_listen();
        return true;
    }

    bool server_event_socket::close() {
        event_socket::stop_polling();
        return server_socket::close();
    }

    void server_event_socket::on_data_impl(const char* buffer, int size) {
        (void)buffer;
        (void)size;
    }

    void server_event_socket::on_listen_impl() {
    }

    void server_event_socket::on_close_impl() {
    }

    void server_event_socket::on_error_impl(int error_code) {
        (void)error_code;
    }

}  // namespace net
