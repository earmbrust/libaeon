/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#pragma once

#include "address.h"
#include <string>

namespace net {

    class resolver {
    public:
        resolver();
        ~resolver();

        template<typename T>
        T resolve(const char* hostname);

        std::string resolve_to_string(const char* hostname);
        address resolve_to_address(const char* hostname);

    private:
        address resolve_internal(const char* hostname);
    };

} // namespace net