/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#ifndef _CSOCKET_SET_H
#define _CSOCKET_SET_H

#include "CSocket.h"

namespace net {
    class CSocketSet {
    public:
        CSocketSet() : error_code(ERR_NONE), error_state(0) {}
        
        std::vector<CSocket*> Sockets;
        int error_code;
        int error_state;
        
        bool Add(CSocket* socket_ref);
        [[deprecated("Use Add(CSocket*) instead")]]
        bool Add();
        bool Remove(unsigned int index);
        bool Remove(unsigned int index, unsigned int count);
        int Size() const;
    };
}

#endif // _CSOCKET_SET_H
