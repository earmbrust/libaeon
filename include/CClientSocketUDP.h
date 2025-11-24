/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#ifndef _CCLIENT_SOCKET_UDP_H
#define _CCLIENT_SOCKET_UDP_H

#include "CSocketUDP.h"

namespace net {
    class CClientSocketUDP : public CSocketUDP {
    public:
        CClientSocketUDP();
        CClientSocketUDP(const char* hostname, int port);
        CClientSocketUDP(std::string* hostname, int port);
        ~CClientSocketUDP();
        bool Connect();
        bool Connect(const char* hostname, int port);
        bool Connect(const CAddress& addr);
    protected:
        struct sockaddr_storage serv_addr;
        struct addrinfo* server;
    };
}

#endif // _CCLIENT_SOCKET_UDP_H
