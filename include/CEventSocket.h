/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#ifndef _CEVENT_SOCKET_H
#define _CEVENT_SOCKET_H

#include "CSocket.h"

namespace net {
    class CEventSocket : public CSocket {
    public:
        CEventSocket() {}
        explicit CEventSocket(socket_t existing_fd) : CSocket(existing_fd, true) {}
        virtual bool OnRead(const char* buffer, int size);
        virtual void OnWrite(const char* buffer, int size, int sentsize);
        int Write(char* data);
        int Write(const char* data);
        int Write(const std::string& data);
        bool Poll();
    };
}

#endif // _CEVENT_SOCKET_H
