/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#pragma once

#ifdef _MSC_VER
// C4251: STL member needs dll-interface
// This is a known MSVC artifact when exporting classes with STL containers.
// The vectors and functions are explicitly instantiated in .cpp files with dllexport,
// so the warning about missing interface is spurious. Safe to suppress globally.
#pragma warning(disable:4251)
#endif

#include "internal/config.hpp"

#include "socket.hpp"
#include "address.hpp"
#include "resolver.hpp"
#include "client_socket.hpp"
#include "server_socket.hpp"
#include "event_socket.hpp"
#include "client_event_socket.hpp"
#include "server_event_socket.hpp"
#include "event_socket_set.hpp"
#include "socket_set.hpp"
#include "udp_socket.hpp"
#include "udp_client_socket.hpp"
#include "udp_server_socket.hpp"

#ifdef ENABLE_SSL
#include "ssl_socket.hpp"
#include "ssl_client_socket.hpp"
#endif

namespace aeon {

    const char* get_library_version();

} // aeon