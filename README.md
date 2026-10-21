# Aeon (`libaeon`)

Aeon is a lightweight, cross-platform **C++17** networking library for TCP,
UDP, socket events, IPv4/IPv6 addressing and optional OpenSSL/TLS support.
**`aeon` is the C++ namespace; `libaeon` is the library file name.**

## Public API and naming

The 2.0 API follows the C++ Core Guidelines' preferred `underscore_style` for
namespaces, classes, functions and methods. The generic `aeon::socket` is an
extensible base class, replacing the original pre-2.0 `net::CSocket` spelling.

```cpp
#include <aeon.hpp>

aeon::client_socket client;
if (client.connect("127.0.0.1", 2300)) {
    client.set_read_timeout(1000);
    client.write("Hello from Aeon!\n");
    client.close();
}
```

Other public types include `aeon::server_socket`, `aeon::udp_socket`,
`aeon::udp_client_socket`, `aeon::udp_server_socket`, `aeon::event_socket`,
`aeon::socket_set`, `aeon::event_socket_set`, `aeon::address` and
`aeon::resolver`. SSL-enabled builds also expose `aeon::ssl_socket` and
`aeon::ssl_client_socket`.

### TCP server

```cpp
#include <aeon.hpp>

aeon::server_socket server;
if (server.listen(2300)) {
    auto client = server.accept(); // std::unique_ptr<aeon::event_socket>
    if (client) {
        client->write("Hello from server!\n");
    }
}
```

### UDP receive with a timeout

```cpp
#include <aeon.hpp>

aeon::udp_server_socket server;
if (server.listen(2301)) {
    server.set_read_timeout(1000); // milliseconds
    char buffer[256]{};
    const int count = server.read(buffer, sizeof(buffer));
    if (count > 0) {
        server.write(buffer, count); // Reply to sender
    }
}
```

On timed-out UDP reads, the buffer overload returns `0` and the string overload
returns an empty string. A zero-length UDP datagram is valid and does **not**
indicate a disconnected peer. For application protocols, distinguish a timeout
from a zero-length datagram at the protocol level if that matters.

## Build and test

Linux and macOS (GCC / Clang):

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Windows (MSVC 2022):

```powershell
cmake -S . -B build-vs2022 -G "Visual Studio 17 2022" -A x64
cmake --build build-vs2022 --config Release
ctest --test-dir build-vs2022 -C Release --output-on-failure
```

Visual Studio 2026 is also supported with the Visual Studio 18 generator in a
sufficiently new CMake. To enable TLS, install a matching OpenSSL development
package and configure with `-DENABLE_SSL=ON`.

Builds include Google Test unit tests, fetched during CMake configure.
A public CI matrix builds on Windows, Linux and macOS with several compilers
and architectures (including ARM and 32-bit targets). See
[GitHub Actions](https://github.com/earmbrust/libaeon/actions) for current
results; a configured platform is not automatically a passing platform.

## Documentation and examples

- [Documentation sources](docs/)
- [C++ examples](example/)
- [GitHub repository](https://github.com/earmbrust/libaeon)

The documentation build uses Doxygen, Breathe and Sphinx. The code examples
and API reference target Aeon 2.0, not the older `net::C*` class names.

## License

BSD 3-Clause. See the project license and copyright notices.
