/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#ifndef _CEVENT_SOCKET_SET_H
#define _CEVENT_SOCKET_SET_H

#include "CEventSocket.h"

namespace net {
    class CEventSocketSet {
    public:
        CEventSocketSet() : error_code(ERR_NONE), error_state(0) {}
        ~CEventSocketSet() { this->Cleanup(); }
        
        std::vector<CEventSocket*> Sockets;
        int error_code;
        int error_state;
        
        bool Add(CEventSocket* socket_ref);
        [[deprecated("Use Add(CEventSocket*) instead")]]
        bool Add();
        bool Remove(unsigned int index);
        bool Remove(unsigned int index, unsigned int count);
        int Size() const;
        void Poll();
        void Cleanup();
    };
}

#endif // _CEVENT_SOCKET_SET_H
