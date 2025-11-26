/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#pragma once

#include "internal/config.h"
#include "internal/export.h"
#include <string>
#include <cstdint>

namespace net {

    class LIBAEON_API address {
    public:
        address();
        explicit address(const sockaddr_storage& src_addr);
        explicit address(const sockaddr_in& src_addr);
        explicit address(const sockaddr_in6& src_addr);

        bool is_ipv4() const;
        bool is_ipv6() const;

        std::string get_string() const;
        std::uint16_t get_port() const;
        void set_port(std::uint16_t port);

        sockaddr_storage get_sockaddr_storage() const;
        sockaddr_in get_sockaddr_ipv4() const;
        sockaddr_in6 get_sockaddr_ipv6() const;

    private:
        sockaddr_storage addr;
        int family;
    };

} // namespace net

