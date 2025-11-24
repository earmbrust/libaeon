/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#ifndef _CSERVER_SOCKET_UDP_H
#define _CSERVER_SOCKET_UDP_H

#include "CSocketUDP.h"

namespace net {
    class CServerSocketUDP : public CSocketUDP {
    public:
        CServerSocketUDP();
        ~CServerSocketUDP();
        bool Listen();
        bool Listen(int port);
    protected:
        struct sockaddr_storage serv_addr;
    };
}

#endif // _CSERVER_SOCKET_UDP_H
