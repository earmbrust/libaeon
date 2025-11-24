/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#ifndef _CADDRESS_H
#define _CADDRESS_H

#include "libaeon-common.h"

namespace net {
    /**
     * \brief A portable address class that encapsulates IPv4/IPv6 addresses with ports
     * Stores sockaddr_storage internally and provides convenient access methods
     */
    class CAddress {
    public:
        CAddress();
        CAddress(const sockaddr_storage& addr);
        CAddress(const sockaddr_in& addr);
        CAddress(const sockaddr_in6& addr);
        
        bool IsIPv4() const;
        bool IsIPv6() const;
        
        std::string GetString() const;
        uint16_t GetPort() const;
        void SetPort(uint16_t port);
        
        sockaddr_storage GetSockaddrStorage() const;
        sockaddr_in GetSockaddrIPv4() const;    // Throws if not IPv4
        sockaddr_in6 GetSockaddrIPv6() const;   // Throws if not IPv6
        
    private:
        sockaddr_storage addr;
        int family;  // AF_INET or AF_INET6
    };
}

#endif // _CADDRESS_H
