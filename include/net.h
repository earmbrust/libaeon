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
#include "client_event_socket.h"
#include "server_event_socket.h"
#include "event_socket_set.h"
#include "socket_set.h"
#include "udp_socket.h"
#include "udp_client_socket.h"
#include "udp_server_socket.h"

#ifdef ENABLE_SSL
#include "ssl_socket.h"
#include "ssl_client_socket.h"
#endif

namespace net {

    const char* get_library_version();

} // namespace net