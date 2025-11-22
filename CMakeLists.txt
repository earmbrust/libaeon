cmake_minimum_required(VERSION 3.15)
project(libaeon VERSION 0.18.3 LANGUAGES CXX)

# C++17 standard required
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_definitions(-DVERSION=${PROJECT_VERSION})

# Platform detection
if(WIN32)
    message(STATUS "Detected platform: Windows")
    add_definitions(-D_WIN32)
elseif(APPLE)
    message(STATUS "Detected platform: macOS")
    add_definitions(-D__APPLE__)
elseif(UNIX AND NOT APPLE)
    message(STATUS "Detected platform: Linux")
    add_definitions(-D__linux__)
endif()

# Architecture detection and logging
message(STATUS "Target architecture: ${CMAKE_SYSTEM_PROCESSOR}")
if(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM|arm|aarch64")
    message(STATUS "ARM architecture detected - optimization may vary")
endif()

# Configuration header
include(CheckIncludeFiles)
CHECK_INCLUDE_FILES(dlfcn.h HAVE_DLFCN_H)
CHECK_INCLUDE_FILES(inttypes.h HAVE_INTTYPES_H)
CHECK_INCLUDE_FILES(memory.h HAVE_MEMORY_H)
CHECK_INCLUDE_FILES(stdbool.h HAVE_STDBOOL_H)
CHECK_INCLUDE_FILES(stdint.h HAVE_STDINT_H)
CHECK_INCLUDE_FILES(stdlib.h HAVE_STDLIB_H)
CHECK_INCLUDE_FILES(strings.h HAVE_STRINGS_H)
CHECK_INCLUDE_FILES(string.h HAVE_STRING_H)

# Include directories
include_directories(src)

# Main library sources
set(LIBAEON_SOURCES
    src/libaeon.cpp
)

# Shared library
add_library(aeon SHARED ${LIBAEON_SOURCES})

# Static library
add_library(aeon-static STATIC ${LIBAEON_SOURCES})
set_target_properties(aeon-static PROPERTIES OUTPUT_NAME aeon)

# Platform-specific linking
if(WIN32)
    target_link_libraries(aeon PUBLIC ws2_32)
    target_link_libraries(aeon-static PUBLIC ws2_32)
endif()

# Compiler warnings and optimizations
if(MSVC)
    # MSVC compiler options
    target_compile_options(aeon PRIVATE /W4 /wd4458)
    target_compile_options(aeon-static PRIVATE /W4 /wd4458)
    
    # Disable secure warnings
    target_compile_definitions(aeon PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_definitions(aeon-static PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    # GCC/Clang compiler options
    target_compile_options(aeon PRIVATE -Wall -Wextra -Wconversion -Wpedantic)
    target_compile_options(aeon-static PRIVATE -Wall -Wextra -Wconversion -Wpedantic)
    
    # Optimization flags: only use -march=native for native builds
    # Skip it for cross-compilation (including ARM targets)
    if(NOT CMAKE_CROSSCOMPILING)
        target_compile_options(aeon PRIVATE -march=native)
        target_compile_options(aeon-static PRIVATE -march=native)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM|arm|aarch64")
        # For ARM cross-compilation, users can specify architecture via ARM_ARCH
        # Example: cmake -DARM_ARCH=armv7-a
        if(DEFINED ARM_ARCH)
            message(STATUS "ARM architecture specified: ${ARM_ARCH}")
            target_compile_options(aeon PRIVATE -march=${ARM_ARCH})
            target_compile_options(aeon-static PRIVATE -march=${ARM_ARCH})
        else()
            message(STATUS "ARM cross-compilation detected. No -march flag applied.")
            message(STATUS "To specify ARM architecture, use: cmake -DARM_ARCH=armv7-a (or armv8-a, etc.)")
        endif()
    endif()
endif()

# Example executables
add_executable(http-client example/http-client.cpp)
target_link_libraries(http-client aeon)

add_executable(hello-world example/hello-world.cpp)
target_link_libraries(hello-world aeon)

add_executable(simple-server example/simple-server.cpp)
target_link_libraries(simple-server aeon)

add_executable(test-client example/test-client.cpp)
target_link_libraries(test-client aeon)

add_executable(udp-server example/udp-server.cpp)
target_link_libraries(udp-server aeon)

add_executable(udp-client example/udp-client.cpp)
target_link_libraries(udp-client aeon)

# Build configuration summary
message(STATUS "")
message(STATUS "libaeon Build Configuration")
message(STATUS "===========================")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "Processor: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "")