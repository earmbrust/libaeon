/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#ifndef _CCLIENT_SOCKET_H
#define _CCLIENT_SOCKET_H

#include "CSocket.h"

namespace net {
    class CClientSocket : public CSocket {
    public:
        bool Connect();
        bool Connect(const char* remote, int port);
        bool Connect(const CAddress& addr);
        CClientSocket();
        CClientSocket(std::string *remote, int port);
        CClientSocket(const char* remote, int port);
        ~CClientSocket();
    protected:
        struct sockaddr_in serv_addr;
        struct addrinfo *server;
    };
}

#endif // _CCLIENT_SOCKET_H
