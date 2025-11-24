/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#ifndef _LIBAEON_H
#define _LIBAEON_H

/*!
 \file libaeon.h
 \author Elden Armbrust
 \brief The main libaeon include file.
 libaeon.h is the main include file for both libaeon developers and
 developers looking to build against libaeon.
 */

/**
 * \namespace net
 * \author Elden Armbrust
 * \brief The container namespace for all libaeon related classes and methods
 *
 * The net namespace encapsulates all network communications of libaeon
 * to prevent name collision with other implementations.
 */

// Include individual class headers
#include "libaeon-common.h"
#include "CAddress.h"
#include "CResolver.h"
#include "CSocket.h"
#include "CClientSocket.h"
#include "CServerSocket.h"
#include "CEventSocket.h"
#include "CSocketSet.h"
#include "CEventSocketSet.h"
#include "CSocketUDP.h"
#include "CClientSocketUDP.h"
#include "CServerSocketUDP.h"

namespace net {
    // Derived event classes
    class CEventClientSocket : public CClientSocket {
    };

    class CEventServerSocket : public CServerSocket, public CEventSocket {
    };

    const char* GetLibraryVersion();
}

#endif // _LIBAEON_H
