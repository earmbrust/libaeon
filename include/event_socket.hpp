/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#pragma once

#include "socket.hpp"
#include <thread>
#include <functional>

namespace aeon {

    class LIBAEON_API event_socket : public socket {
    public:
        event_socket();
        virtual ~event_socket();

        int write(const char* data);
        int write(const std::string& data);

        // Public callbacks - users assign these
        std::function<void(const char*, int)> on_data;
        std::function<void()> on_connect;
        std::function<void()> on_close;
        std::function<void()> on_listen;
        std::function<void(int)> on_error;

    protected:
        // Virtual methods - derived classes can override these
        virtual void on_data_impl(const char* buffer, int size);
        virtual void on_connect_impl();
        virtual void on_close_impl();
        virtual void on_listen_impl();
        virtual void on_error_impl(int error_code);

        void start_polling();
        void stop_polling();
        
        void fire_on_data(const char* buffer, int size);
        void fire_on_connect();
        void fire_on_close();
        void fire_on_listen();
        void fire_on_error(int error_code);

    private:
        std::thread* poll_thread_;
        bool polling_;

        void poll();
        void polling_loop();
    };

} // aeon

#ifdef _MSC_VER
template class __declspec(dllexport) std::function<void(const char*, int)>;
template class __declspec(dllexport) std::function<void()>;
template class __declspec(dllexport) std::function<void(int)>;
#endif




