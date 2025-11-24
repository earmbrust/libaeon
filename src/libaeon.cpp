/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#include "libaeon.h"

#include "CAddress.cpp"
#include "CResolver.cpp"
#include "CSocket.cpp"
#include "CClientSocket.cpp"
#include "CServerSocket.cpp"

#include "CSocketUDP.cpp"
#include "CServerSocketUDP.cpp"
#include "CClientSocketUDP.cpp"
#include "CEventSocket.cpp"
#include "CSocketSet.cpp"
#include "CEventSocketSet.cpp"

// Stringification macros to convert version numbers to string
#define STR_(x) #x
#define STR(x) STR_(x)

namespace net {
    const char* GetLibraryVersion() {
        return STR(VERSION);
    };
}