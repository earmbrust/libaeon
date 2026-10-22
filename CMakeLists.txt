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

# C++20 is the public API and consumer baseline. C++23 is an explicitly
# selected implementation profile, never a requirement of installed headers.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(LIBAEON_ENABLE_CXX23 "Build Aeon internals in C++23 mode when supported (public API remains C++20)" OFF)
option(LIBAEON_ENABLE_NATIVE_ARCH "Use -march=native for local benchmarks only (not redistributable builds)" OFF)

set(LIBAEON_IMPLEMENTATION_CXX_STANDARD 20)
if(LIBAEON_ENABLE_CXX23)
    if(CMAKE_VERSION VERSION_LESS 3.20)
        message(FATAL_ERROR "LIBAEON_ENABLE_CXX23 requires CMake >= 3.20")
    endif()

    include(CheckCXXSourceCompiles)
    # Check the actual selected C++23 compiler mode, not just the compiler version.
    # Some toolchains provide only a preview dialect, and MSVC has _MSVC_LANG.
    set(CMAKE_CXX_STANDARD 23)
    check_cxx_source_compiles("
#if defined(_MSVC_LANG)
#  if _MSVC_LANG <= 202002L
#    error C++23 mode was not selected
#  endif
#elif __cplusplus <= 202002L
#  error C++23 mode was not selected
#endif
int main() { return 0; }
" LIBAEON_CXX23_MODE_WORKS)
    if(NOT LIBAEON_CXX23_MODE_WORKS)
        message(FATAL_ERROR "C++23 was requested, but the current compiler cannot build in C++23 mode. Set LIBAEON_ENABLE_CXX23=OFF to use the supported C++20 baseline.")
    endif()

    # Probe individual library features with real compile/link tests. Do not
    # infer feature support from -std=c++23 or change public ABI per feature.
    check_cxx_source_compiles("
#include <version>
#include <expected>
#if !defined(__cpp_lib_expected) || __cpp_lib_expected < 202202L
#  error std::expected unavailable
#endif
int main() { std::expected<int, int> x(42); return x.value() == 42 ? 0 : 1; }
" LIBAEON_HAVE_CXX23_EXPECTED)
    check_cxx_source_compiles("
#include <version>
#include <bit>
#include <cstdint>
#if !defined(__cpp_lib_byteswap) || __cpp_lib_byteswap < 202110L
#  error std::byteswap unavailable
#endif
int main() { return std::byteswap(std::uint32_t{0x12345678}) == 0x78563412 ? 0 : 1; }
" LIBAEON_HAVE_CXX23_BYTESWAP)
    check_cxx_source_compiles("
#include <version>
#include <utility>
#if !defined(__cpp_lib_to_underlying) || __cpp_lib_to_underlying < 202102L
#  error std::to_underlying unavailable
#endif
enum class e : int { value = 7 };
int main() { return std::to_underlying(e::value) == 7 ? 0 : 1; }
" LIBAEON_HAVE_CXX23_TO_UNDERLYING)
    set(CMAKE_CXX_STANDARD 20)
    set(LIBAEON_IMPLEMENTATION_CXX_STANDARD 23)
endif()

add_definitions(-DVERSION=${PROJECT_VERSION})

# SSL/TLS support option
option(ENABLE_SSL "Enable SSL/TLS support with OpenSSL" OFF)

if(ENABLE_SSL)
    find_package(OpenSSL REQUIRED)
    message(STATUS "SSL/TLS support enabled with OpenSSL ${OPENSSL_VERSION}")
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
    src/aeon.cpp
    src/address.cpp
    src/resolver.cpp
    src/socket.cpp
    src/internal/tcp_ops.cpp
    src/internal/native_poller.cpp
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
    src/udp_event_socket.cpp
    src/udp_client_event_socket.cpp
    src/udp_server_event_socket.cpp
    src/internal/udp_ops.cpp
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
target_compile_definitions(aeon-static PUBLIC LIBAEON_STATIC)

# Consumers always require C++20, including when library internals are built
# with optional C++23 features. Never propagate cxx_std_23 through targets.
target_compile_features(aeon PUBLIC cxx_std_20)
target_compile_features(aeon-static PUBLIC cxx_std_20)
set_target_properties(aeon aeon-static PROPERTIES
    CXX_STANDARD ${LIBAEON_IMPLEMENTATION_CXX_STANDARD}
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# SSL support is part of the public header contract, not just an internal setting.
# Propagate it to programs that link either target through CMake.
if(ENABLE_SSL)
    target_compile_definitions(aeon PUBLIC ENABLE_SSL)
    target_compile_definitions(aeon-static PUBLIC ENABLE_SSL)
endif()

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
    
    # -march=native can make redistributed binaries fail on other CPUs.
    # Only enable it for explicit local benchmark builds, never cross-builds.
    if(NOT CMAKE_CROSSCOMPILING)
        if(LIBAEON_ENABLE_NATIVE_ARCH)
            target_compile_options(aeon PRIVATE -march=native)
            target_compile_options(aeon-static PRIVATE -march=native)
        endif()
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

# BUILD_TESTING defaults to ON; OFF supports dependency-free library builds.
# Normal test configurations retain the existing GoogleTest suite and consumers.
include(CTest)
if(BUILD_TESTING)
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
    tests/test_tcp_ownership.cpp
    tests/test_event_callbacks.cpp
    tests/test_event_lifecycle.cpp
    tests/test_event_polling.cpp
    tests/test_udp_ownership.cpp
    tests/test_udp.cpp
    tests/test_resolver.cpp
)

target_include_directories(aeon_tests PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(aeon_tests PRIVATE aeon gtest_main)

# Register tests with ctest
add_test(NAME AeonTests COMMAND aeon_tests)

# Internal implementation tests link the static library so non-exported
# implementation seams are testable without adding DLL/public API symbols.
add_executable(aeon_internal_tests
    tests/test_udp_internal.cpp
    tests/test_callback_state_internal.cpp
    tests/test_socket_registry_internal.cpp
    tests/test_native_poller_internal.cpp
)
target_include_directories(aeon_internal_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(aeon_internal_tests PRIVATE aeon-static gtest_main)
add_test(NAME AeonInternalTests COMMAND aeon_internal_tests)

# Compiled, independently linked consumers validate the public API and ABI-facing
# export configuration, not merely compilation of internal unit tests.
add_executable(aeon_consumer_shared tests/consumer_interface.cpp)
target_link_libraries(aeon_consumer_shared PRIVATE aeon)
add_test(NAME AeonSharedConsumer COMMAND aeon_consumer_shared)

add_executable(aeon_consumer_static tests/consumer_interface.cpp)
target_link_libraries(aeon_consumer_static PRIVATE aeon-static)
add_test(NAME AeonStaticConsumer COMMAND aeon_consumer_static)

# Compile and run the public interface in strict C++20 even when implementation
# builds use C++23. Covers both shared and static export/propagation behavior.
add_executable(aeon_cxx20_shared tests/m0_cpp_baseline.cpp)
target_link_libraries(aeon_cxx20_shared PRIVATE aeon)
set_target_properties(aeon_cxx20_shared PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
add_test(NAME AeonCxx20Shared COMMAND aeon_cxx20_shared)

add_executable(aeon_cxx20_static tests/m0_cpp_baseline.cpp)
target_link_libraries(aeon_cxx20_static PRIVATE aeon-static)
set_target_properties(aeon_cxx20_static PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
add_test(NAME AeonCxx20Static COMMAND aeon_cxx20_static)
endif()

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
message(STATUS "Public C++ Standard: 20")
message(STATUS "Implementation C++ Standard: ${LIBAEON_IMPLEMENTATION_CXX_STANDARD}")
message(STATUS "Local -march=native: ${LIBAEON_ENABLE_NATIVE_ARCH}")
if(LIBAEON_ENABLE_CXX23)
    message(STATUS "C++23 std::expected: ${LIBAEON_HAVE_CXX23_EXPECTED}")
    message(STATUS "C++23 std::byteswap: ${LIBAEON_HAVE_CXX23_BYTESWAP}")
    message(STATUS "C++23 std::to_underlying: ${LIBAEON_HAVE_CXX23_TO_UNDERLYING}")
endif()
message(STATUS "Build tests: ${BUILD_TESTING}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "Processor: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "")