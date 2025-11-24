/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#pragma once

#include "event_socket.h"
#include <vector>

namespace net {

    class event_socket_set {
    public:
        bool add(event_socket* socket_ref);
        bool remove(unsigned int index);
        bool remove(unsigned int index, unsigned int count);

        int size() const;
        void poll();
        void cleanup();

    private:
        std::vector<event_socket*> sockets_;
        int error_code_;
        int error_state_;
    };

} // namespace net
