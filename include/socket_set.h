/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#pragma once

#include "socket.h"
#include <vector>

namespace net {

    class socket_set {
    public:
        bool add(socket* sock_ref);
        bool remove(unsigned int index);
        bool remove(unsigned int index, unsigned int count);

        int size() const;

    private:
        std::vector<socket*> sockets_;
        int error_code_;
        int error_state_;
    };

} // namespace net
