#pragma once

#ifdef _WIN32
    #ifdef LIBAEON_STATIC
        // Static library - no export decoration needed
        #define LIBAEON_API
    #elif defined(LIBAEON_EXPORTS)
        // Building shared library - export symbols
        #define LIBAEON_API __declspec(dllexport)
    #else
        // Using shared library - import symbols
        #define LIBAEON_API __declspec(dllimport)
    #endif
#else
    // On non-Windows platforms, no special export needed
    #define LIBAEON_API
#endif