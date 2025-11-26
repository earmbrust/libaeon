cmake_minimum_required(VERSION 3.15)

# Set policy for FetchContent timestamp handling (CMake 3.24+)
if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()

# Fix Python3 detection for Windows + pyenv
# Google Test doesn't need Python to build, but its CMake tries to find it
# On Windows, pyenv shims don't work with CMake's FindPython, so detect real path
if(WIN32 AND NOT Python3_EXECUTABLE)
  find_program(PYENV_EXE NAMES pyenv.bat pyenv)
  if(PYENV_EXE)
    execute_process(
      COMMAND "${PYENV_EXE}" which python3
      OUTPUT_VARIABLE PYENV_PYTHON
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET
    )
    if(PYENV_PYTHON)
      set(Python3_EXECUTABLE "${PYENV_PYTHON}" CACHE FILEPATH "Python 3 executable" FORCE)
    endif()
  endif()
endif()

# Get version from git tags, fall back to branch name
execute_process(
    COMMAND git describe --tags --always
    OUTPUT_VARIABLE GIT_DESCRIBE
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# Parse the git describe output to get clean version
# Format can be: 0.16.22-79-g184ecca or v1.18.0 or just commit hash
if(GIT_DESCRIBE MATCHES "^(v)?([0-9]+\\.[0-9]+\\.[0-9]+)")
    # Extract the version part (before any -commits-ghash)
    string(REGEX MATCH "^(v)?([0-9]+\\.[0-9]+\\.[0-9]+)" VERSION_MATCH "${GIT_DESCRIBE}")
    string(REGEX REPLACE "^v" "" PROJECT_VERSION "${VERSION_MATCH}")
    message(STATUS "Version from git tag: ${PROJECT_VERSION}")
else()
    # Not a version tag, try to get branch name
    execute_process(
        COMMAND git rev-parse --abbrev-ref HEAD
        OUTPUT_VARIABLE GIT_BRANCH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    
    if(GIT_BRANCH AND NOT GIT_BRANCH STREQUAL "HEAD")
        # Extract numeric version from branch name (e.g., "1.18-devel" -> "1.18.0")
        string(REGEX MATCH "^[0-9]+(\\.[0-9]+)*" VERSION_PART "${GIT_BRANCH}")
        
        if(VERSION_PART)
            # Count dots to pad to X.Y.Z format
            string(REGEX MATCHALL "\\." DOT_COUNT "${VERSION_PART}")
            list(LENGTH DOT_COUNT NUM_DOTS)
            
            if(NUM_DOTS EQUAL 1)
                # X.Y format, add .0
                set(PROJECT_VERSION "${VERSION_PART}.0")
            elseif(NUM_DOTS EQUAL 2)
                # Already X.Y.Z format
                set(PROJECT_VERSION "${VERSION_PART}")
            else()
                # X format or other, add .0.0
                set(PROJECT_VERSION "${VERSION_PART}.0.0")
            endif()
            
            set(GIT_VERSION_STRING "${GIT_BRANCH}")
            message(STATUS "Version from git branch: ${PROJECT_VERSION} (branch: ${GIT_BRANCH})")
        else()
            set(PROJECT_VERSION "0.0.0")
            message(STATUS "Could not extract numeric version from branch: ${GIT_BRANCH}")
        endif()
    else()
        set(PROJECT_VERSION "0.0.0")
        message(STATUS "Version set to default: ${PROJECT_VERSION}")
    endif()
endif()

# Store full git describe for display/info purposes
set(GIT_VERSION_STRING "${GIT_DESCRIBE}")
message(STATUS "Git version string: ${GIT_VERSION_STRING}")

# Now create project with version from git
project(libaeon VERSION ${PROJECT_VERSION} LANGUAGES CXX)

# C++17 standard required
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_definitions(-DVERSION=${PROJECT_VERSION})

# SSL/TLS support option
option(ENABLE_SSL "Enable SSL/TLS support with OpenSSL" OFF)

if(ENABLE_SSL)
    find_package(OpenSSL REQUIRED)
    message(STATUS "SSL/TLS support enabled with OpenSSL ${OPENSSL_VERSION}")
    add_definitions(-DENABLE_SSL)
else()
    message(STATUS "SSL/TLS support disabled")
endif()

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

# Include directories - headers are in include/, sources in src/
include_directories(include)

# Main library sources - list all implementation files
set(LIBAEON_SOURCES
    src/libaeon.cpp
    src/address.cpp
    src/resolver.cpp
    src/socket.cpp
    src/client_socket.cpp
    src/server_socket.cpp
    src/event_socket.cpp
    src/client_event_socket.cpp
    src/server_event_socket.cpp
    src/socket_set.cpp
    src/event_socket_set.cpp
    src/udp_socket.cpp
    src/udp_client_socket.cpp
    src/udp_server_socket.cpp
)

# Add SSL sources if enabled
if(ENABLE_SSL)
    list(APPEND LIBAEON_SOURCES
        src/ssl_socket.cpp
        src/ssl_client_socket.cpp
    )
endif()

# Shared library
add_library(aeon SHARED ${LIBAEON_SOURCES})
target_compile_definitions(aeon PRIVATE LIBAEON_EXPORTS)

# Static library
add_library(aeon-static STATIC ${LIBAEON_SOURCES})
set_target_properties(aeon-static PROPERTIES OUTPUT_NAME aeon_static)
target_compile_definitions(aeon-static PRIVATE LIBAEON_STATIC)

# Platform-specific linking
if(WIN32)
    target_link_libraries(aeon PUBLIC ws2_32)
    target_link_libraries(aeon-static PUBLIC ws2_32)
endif()

# SSL/TLS linking
if(ENABLE_SSL)
    target_link_libraries(aeon PUBLIC OpenSSL::SSL OpenSSL::Crypto)
    target_link_libraries(aeon-static PUBLIC OpenSSL::SSL OpenSSL::Crypto)
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

# Testing with Google Test
include(FetchContent)

FetchContent_Declare(googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
  DOWNLOAD_EXTRACT_TIMESTAMP ON
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

enable_testing()

# Test executable
add_executable(aeon_tests
    tests/test_address.cpp
    tests/test_socket_basics.cpp
    tests/test_udp.cpp
    tests/test_resolver.cpp
)

target_include_directories(aeon_tests PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(aeon_tests PRIVATE aeon gtest_main)

# Register tests with ctest
add_test(NAME AeonTests COMMAND aeon_tests)

# Example executables
add_executable(http-client example/http-client.cpp)
target_include_directories(http-client PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(http-client aeon)

add_executable(hello-world example/hello-world.cpp)
target_include_directories(hello-world PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(hello-world aeon)

add_executable(simple-server example/simple-server.cpp)
target_include_directories(simple-server PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(simple-server aeon)

add_executable(test-client example/test-client.cpp)
target_include_directories(test-client PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test-client aeon)

add_executable(udp-server example/udp-server.cpp)
target_include_directories(udp-server PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(udp-server aeon)

add_executable(udp-client example/udp-client.cpp)
target_include_directories(udp-client PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(udp-client aeon)

# Build configuration summary
message(STATUS "")
message(STATUS "libaeon Build Configuration")
message(STATUS "===========================")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "Processor: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "")