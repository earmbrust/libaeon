/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#ifndef _CRESOLVER_H
#define _CRESOLVER_H

#include "CAddress.h"

namespace net {
    /**
     * \brief DNS resolver for hostname to address resolution
     * Forward lookup only (hostname -> IP address)
     * Prefers IPv6 if available, otherwise returns IPv4
     */
    class CResolver {
    public:
        CResolver();
        ~CResolver();
        
        // Template-based resolution - specializations for different return types
        template<typename T>
        T Resolve(const char* hostname);
        
    private:
        // Helper to do the actual resolution
        CAddress ResolveInternal(const char* hostname);
    };
}

#endif // _CRESOLVER_H
