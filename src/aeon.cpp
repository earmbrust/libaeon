/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#include <aeon.hpp>

// Stringification helpers for the public aeon::get_library_version() API.
#define STR_(x) #x
#define STR(x) STR_(x)

namespace aeon {
    const char* get_library_version() {
        return STR(VERSION);
    };
}
