/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#include <aeon.hpp>

// Stringification macros to convert version numbers to string
#define STR_(x) #x
#define STR(x) STR_(x)

namespace aeon {
    const char* GetLibraryVersion() {
        return STR(VERSION);
    };
}
