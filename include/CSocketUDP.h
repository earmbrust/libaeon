/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#ifndef _CSOCKET_UDP_H
#define _CSOCKET_UDP_H

#include "CSocket.h"

namespace net {
    class CSocketUDP : public CSocket {
    public:
        CSocketUDP();
        int Write(char* data, int size);
        int Write(const char* data, int size);
        int Write(char* data);
        int Write(const char* data);
        int Write(const std::string& data);
        int Read();
        int Read(char* buffer, int size);
        int ReadUntil(char* buffer, int size);
        std::string Read(int size);
        static const int DefaultSocketType = SOCK_DGRAM;
    protected:
        socklen_t GetRemoteAddrLen() const;
    };
}

#endif // _CSOCKET_UDP_H
