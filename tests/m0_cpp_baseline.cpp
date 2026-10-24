// M0: public-header and link smoke test for the minimum C++20 contract.
// This target is pinned to C++20 even in C++23 implementation builds.
#include <aeon.hpp>
#include <array>
#include <cstddef>
#include <concepts>
#include <memory>
#include <span>
#include <string_view>
#include <type_traits>

#if defined(_MSVC_LANG)
static_assert(_MSVC_LANG >= 202002L, "Aeon requires C++20");
#else
static_assert(__cplusplus >= 202002L, "Aeon requires C++20");
#endif

static_assert(std::derived_from<aeon::client_socket, aeon::socket>);
static_assert(std::derived_from<aeon::server_socket, aeon::socket>);
#if LIBAEON_HAS_EVENTS
static_assert(std::derived_from<aeon::event_socket, aeon::socket>);
#endif
static_assert(std::derived_from<aeon::udp_socket, aeon::socket>);
#ifdef ENABLE_SSL
static_assert(std::derived_from<aeon::ssl_client_socket, aeon::socket>);
#endif

int main() {
    std::array<std::byte, 4> bytes{};
    std::span<std::byte> writable(bytes);
    writable[0] = std::byte{0x42};
    const std::span<const std::byte> readable = writable;
    if (readable.size() != 4 || readable[0] != std::byte{0x42}) {
        return 1;
    }

    const char* version = aeon::get_library_version();
    if (!version || std::string_view(version).empty()) {
        return 2;
    }

    // A constructible public type establishes real compiled consumer linkage.
    aeon::socket_set sockets;
    return sockets.size() == 0 ? 0 : 3;
}
