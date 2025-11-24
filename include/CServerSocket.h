/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#ifndef _CSERVER_SOCKET_H
#define _CSERVER_SOCKET_H

#include "CSocket.h"
#include "CEventSocket.h"

namespace net {
    class CServerSocket : public CSocket {
    public:
        CServerSocket();
        ~CServerSocket();
        bool Listen();
        bool Listen(int port);
        bool Listen(const char* address, int port);
        std::unique_ptr<CEventSocket> Accept();
        std::unique_ptr<CEventSocket> Accept(bool blocking);
        CEventSocket* Accept(CEventSocket* client_socket, bool blocking = false);
        int SetAcceptTimeout(int timeout_ms);
        int accept_timeout_ms;
    protected:
        struct sockaddr_in serv_addr;
        struct hostent *server;
    };
}

#endif // _CSERVER_SOCKET_H
