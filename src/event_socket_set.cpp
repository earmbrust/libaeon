/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/
#include <aeon.hpp>
namespace aeon {
    event_socket_set::event_socket_set()
        : error_code_(err_none), error_state_(0) {
    }
    event_socket_set::~event_socket_set() {
        for (unsigned int i = 0; i < static_cast<unsigned int>(sockets_.size()); ++i) {
            if (sockets_[i]) {
                sockets_[i]->close();
            }
        }
        sockets_.clear();
    }
    bool event_socket_set::add(event_socket* socket) {
        if (!socket) {
            error_code_ = err_no_socket;
            error_state_ = state_create;
            return false;
        }
        sockets_.push_back(socket);
        error_code_ = err_none;
        error_state_ = 0;
        return true;
    }
    bool event_socket_set::remove(unsigned int index) {
        if (index >= static_cast<unsigned int>(sockets_.size())) {
            error_code_ = err_no_socket;
            error_state_ = state_accept;
            return false;
        }
        sockets_.erase(sockets_.begin() + index);
        error_code_ = err_none;
        error_state_ = 0;
        return true;
    }
    int event_socket_set::size() const {
        return static_cast<int>(sockets_.size());
    }
    void event_socket_set::fire_on_socket_data(event_socket* socket, const char* buffer, int size) {
        if (on_socket_data) {
            on_socket_data(socket, buffer, size);
        } else {
            on_socket_data_impl(socket, buffer, size);
        }
    }
    void event_socket_set::fire_on_socket_connect(event_socket* socket) {
        if (on_socket_connect) {
            on_socket_connect(socket);
        } else {
            on_socket_connect_impl(socket);
        }
    }
    void event_socket_set::fire_on_socket_close(event_socket* socket) {
        if (on_socket_close) {
            on_socket_close(socket);
        } else {
            on_socket_close_impl(socket);
        }
    }
    void event_socket_set::fire_on_socket_listen(event_socket* socket) {
        if (on_socket_listen) {
            on_socket_listen(socket);
        } else {
            on_socket_listen_impl(socket);
        }
    }
    void event_socket_set::fire_on_socket_error(event_socket* socket, int error_code) {
        if (on_socket_error) {
            on_socket_error(socket, error_code);
        } else {
            on_socket_error_impl(socket, error_code);
        }
    }
}  // aeon

// Explicit template instantiation for Windows DLL export
#ifdef _MSC_VER
template class __declspec(dllexport) std::vector<aeon::event_socket*>;
template class __declspec(dllexport) std::function<void(aeon::event_socket*, const char*, int)>;
template class __declspec(dllexport) std::function<void(aeon::event_socket*)>;
template class __declspec(dllexport) std::function<void(aeon::event_socket*, int)>;
#endif