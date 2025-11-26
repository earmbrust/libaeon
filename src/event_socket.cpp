/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#include <net.h>
#include "resolver.h"
#include <cstring>
#include <chrono>
#include <stdexcept>

namespace net {

    event_socket::event_socket()
        : socket(), poll_thread_(nullptr), polling_(false) {
        set_blocking(false);
    }

    event_socket::~event_socket() {
        stop_polling();
    }

    void event_socket::start_polling() {
        if (polling_) {
            return;
        }
        polling_ = true;
        poll_thread_ = new std::thread(&event_socket::polling_loop, this);
    }

    void event_socket::stop_polling() {
        polling_ = false;
        if (poll_thread_) {
            poll_thread_->join();
            delete poll_thread_;
            poll_thread_ = nullptr;
        }
    }

    void event_socket::polling_loop() {
        while (polling_) {
            poll();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    void event_socket::poll() {
        if (!this->is_valid_socket()) {
            return;
        }

        this->safe_clear_buffer(this->inbuffer, socket::max_buffer_size);
        int bytesRead = recv(this->sockfd, this->inbuffer, socket::max_buffer_size, 0);
        this->n = bytesRead;

        if (bytesRead > 0) {
            fire_on_data(this->inbuffer, bytesRead);
        } else if (bytesRead < 0) {
            int err = GET_NET_SOCKET_ERROR();
            fire_on_error(err);
        } else if (bytesRead == 0) {
            this->connected = false;
            fire_on_close();
        }
    }

    void event_socket::fire_on_data(const char* buffer, int size) {
        if (on_data) {
            on_data(buffer, size);
        } else {
            on_data_impl(buffer, size);
        }
    }

    void event_socket::fire_on_connect() {
        if (on_connect) {
            on_connect();
        } else {
            on_connect_impl();
        }
    }

    void event_socket::fire_on_close() {
        if (on_close) {
            on_close();
        } else {
            on_close_impl();
        }
    }

    void event_socket::fire_on_listen() {
        if (on_listen) {
            on_listen();
        } else {
            on_listen_impl();
        }
    }

    void event_socket::fire_on_error(int error_code) {
        if (on_error) {
            on_error(error_code);
        } else {
            on_error_impl(error_code);
        }
    }

    void event_socket::on_data_impl(const char* buffer, int size) {
        (void)buffer;
        (void)size;
    }

    void event_socket::on_connect_impl() {
    }

    void event_socket::on_close_impl() {
    }

    void event_socket::on_listen_impl() {
    }

    void event_socket::on_error_impl(int error_code) {
        (void)error_code;
    }

    int event_socket::write(const char* data) {
        if (!data || !this->is_valid_socket()) {
            return NET_SOCKET_ERROR;
        }

        std::size_t len = std::strlen(data);
        if (len == 0) {
            return 0;
        }

        int bytesSent = send(this->sockfd, data, static_cast<int>(len), 0);
        
        if (bytesSent < 0) {
            this->error_code = GET_NET_SOCKET_ERROR();
            fire_on_error(this->error_code);
        }

        return bytesSent;
    }

    int event_socket::write(const std::string& data) {
        if (!this->is_valid_socket() || data.empty()) {
            return NET_SOCKET_ERROR;
        }

        int bytesSent = send(this->sockfd, data.c_str(), static_cast<int>(data.size()), 0);
        
        if (bytesSent < 0) {
            this->error_code = GET_NET_SOCKET_ERROR();
            fire_on_error(this->error_code);
        }

        return bytesSent;
    }

}  // namespace net



