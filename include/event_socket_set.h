/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#pragma once

#ifdef _MSC_VER
// C4251: STL member needs dll-interface
// This is a known MSVC artifact when exporting classes with STL containers.
// The vector is explicitly instantiated in event_socket_set.cpp with dllexport,
// so the warning about missing interface is spurious. Safe to suppress.
#pragma warning(disable:4251)
#endif

#include "event_socket.h"
#include <vector>
#include <functional>

namespace net {

    class LIBAEON_API event_socket_set {
    public:
        event_socket_set();
        virtual ~event_socket_set();

        bool add(event_socket* socket);
        bool remove(unsigned int index);
        int size() const;

        // Public callbacks - users assign these
        std::function<void(event_socket*, const char*, int)> on_socket_data;
        std::function<void(event_socket*)> on_socket_connect;
        std::function<void(event_socket*)> on_socket_close;
        std::function<void(event_socket*)> on_socket_listen;
        std::function<void(event_socket*, int)> on_socket_error;

    protected:
        // Virtual methods - derived classes can override these
        virtual void on_socket_data_impl([[maybe_unused]] event_socket* socket, [[maybe_unused]] const char* buffer, [[maybe_unused]] int size) {}
        virtual void on_socket_connect_impl([[maybe_unused]] event_socket* socket) {}
        virtual void on_socket_close_impl([[maybe_unused]] event_socket* socket) {}
        virtual void on_socket_listen_impl([[maybe_unused]] event_socket* socket) {}
        virtual void on_socket_error_impl([[maybe_unused]] event_socket* socket, [[maybe_unused]] int error_code) {}

        void fire_on_socket_data(event_socket* socket, const char* buffer, int size);
        void fire_on_socket_connect(event_socket* socket);
        void fire_on_socket_close(event_socket* socket);
        void fire_on_socket_listen(event_socket* socket);
        void fire_on_socket_error(event_socket* socket, int error_code);

        std::vector<event_socket*> sockets_;

    private:
        int error_code_;
        int error_state_;
    };

} // namespace net

#ifdef _MSC_VER
template class __declspec(dllexport) std::vector<net::event_socket*>;
template class __declspec(dllexport) std::function<void(net::event_socket*, const char*, int)>;
template class __declspec(dllexport) std::function<void(net::event_socket*)>;
template class __declspec(dllexport) std::function<void(net::event_socket*, int)>;
#endif