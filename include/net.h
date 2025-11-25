/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#pragma once

#include "internal/config.h"

#include "socket.h"
#include "address.h"
#include "resolver.h"
#include "client_socket.h"
#include "server_socket.h"
#include "event_socket.h"
#include "event_socket_set.h"
#include "socket_set.h"
#include "udp_socket.h"
#include "udp_client_socket.h"
#include "udp_server_socket.h"

namespace net {

    class event_client_socket : public client_socket {
    };

    class event_server_socket : public server_socket, public event_socket {
    };

    const char* get_library_version();

} // namespace net
