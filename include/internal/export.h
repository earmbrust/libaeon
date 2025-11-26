#pragma once

#ifdef _WIN32
    #ifdef LIBAEON_EXPORTS
        #define LIBAEON_API __declspec(dllexport)
    #else
        #define LIBAEON_API __declspec(dllimport)
    #endif
#else
    // On non-Windows platforms, no special export needed
    #define LIBAEON_API
#endif