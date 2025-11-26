/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#include "client_event_socket.h"

namespace net {

    client_event_socket::client_event_socket()
        : client_socket(), event_socket() {
    }

    client_event_socket::client_event_socket(const char* hostname, int port)
        : client_socket(hostname, port), event_socket() {
        if (client_socket::connected) {
            event_socket::start_polling();
            fire_on_connect();
        }
    }

    client_event_socket::~client_event_socket() {
        event_socket::stop_polling();
    }

    bool client_event_socket::connect() {
        if (!client_socket::connect()) {
            return false;
        }
        event_socket::start_polling();
        fire_on_connect();
        return true;
    }

    bool client_event_socket::connect(const char* hostname, int port) {
        if (!client_socket::connect(hostname, port)) {
            return false;
        }
        event_socket::start_polling();
        fire_on_connect();
        return true;
    }

    bool client_event_socket::close() {
        event_socket::stop_polling();
        return client_socket::close();
    }

    void client_event_socket::on_data_impl(const char* buffer, int size) {
        (void)buffer;
        (void)size;
    }

    void client_event_socket::on_connect_impl() {
    }

    void client_event_socket::on_close_impl() {
    }

    void client_event_socket::on_error_impl(int error_code) {
        (void)error_code;
    }

}  // namespace net