# Aeon (`libaeon`)

Aeon is a lightweight, cross-platform **C++20** networking library for TCP,
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

Other public types include `aeon::ssl_server_socket` (in SSL-enabled builds),
`aeon::server_socket`, `aeon::udp_socket`,
`aeon::udp_client_socket`, `aeon::udp_server_socket`, `aeon::event_socket`,
`aeon::socket_set`, `aeon::event_socket_set`, `aeon::address` and
`aeon::resolver`. SSL-enabled builds also expose `aeon::ssl_socket` and
`aeon::ssl_client_socket`.

### TCP server

```cpp
#include <aeon.hpp>

aeon::server_socket server;
if (server.listen(2300)) {
    auto* client = server.accept(); // borrowed; server owns accepted TCP peer
    if (client) {
        client->write("Hello from server!\n");
    }
}
```

### TLS server (SSL-enabled builds)

The TLS server reuses the normal TCP listener and owns accepted encrypted
peers. Supply a PEM certificate chain and matching private key before listening:

```cpp
#ifdef ENABLE_SSL
aeon::ssl_server_socket tls;
if (tls.set_certificate_file("server.pem") == aeon::err_none &&
    tls.set_private_key_file("server-key.pem") == aeon::err_none &&
    tls.listen(8443)) {
    if (aeon::ssl_socket* peer = tls.accept(true)) {
        peer->write("secure response\n");
    }
}
#endif
```

TLS acceptance performs a blocking handshake before returning an accepted
peer, with a configurable per-read handshake timeout (5 seconds by default).
Plain TCP servers and clients remain available in the same SSL-enabled build.

### UDP receive with a timeout

```cpp
#include <aeon.hpp>

aeon::udp_server_socket server;
if (server.listen(2301)) {
    server.set_read_timeout(1000); // milliseconds
    char buffer[256]{};
    const int count = server.read(buffer, sizeof(buffer));
    if (count > 0) {
        if (auto* peer = server.get_peer(server.get_remote_address()))
            peer->write(buffer, count); // Reply using server-owned peer
    }
}
```

On timed-out UDP reads, the buffer overload returns `0` and the string overload
returns an empty string. A zero-length UDP datagram is valid and does **not**
indicate a disconnected peer. For application protocols, distinguish a timeout
from a zero-length datagram at the protocol level if that matters.

## Build and test

By default CMake builds only the shared and static libAeon libraries.
Tests, examples, and benchmarks are enabled separately.

Linux and macOS (GCC / Clang):

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Windows (MSVC 2022):

```powershell
cmake -S . -B build-vs2022 -G "Visual Studio 17 2022" -A x64
cmake --build build-vs2022 --config Release
```

Visual Studio 2026 is also supported with the Visual Studio 18 generator in a
sufficiently new CMake. To enable TLS, install a matching OpenSSL development
package and configure with `-DENABLE_SSL=ON`. TLS clients enforce
certificate trust and DNS/IP identity by default; `set_allow_self_signed(true)`
opts into untrusted self-signed leaf certificates without disabling hostname
checking. The HTTP example has a separate explicit `--no-verify` escape hatch.

For tests (which download Google Test on first configuration), enable
`BUILD_TESTING` explicitly:

```sh
cmake -S . -B build-tests -DBUILD_TESTING=ON
cmake --build build-tests --parallel
ctest --test-dir build-tests --output-on-failure
```

To build the example programs, set `LIBAEON_BUILD_EXAMPLES=ON`. To build
the event write-queue benchmark, set `LIBAEON_BUILD_BENCHMARKS=ON`; it also
requires event sockets. All three options default to OFF. CI explicitly
enables tests and examples where needed.

A public CI matrix builds on Windows, Linux and macOS with several compilers
and architectures (including ARM and 32-bit targets). See
[GitHub Actions](https://github.com/earmbrust/libaeon/actions) for current
results; a configured platform is not automatically a passing platform.

### Optional event sockets

Event sockets are enabled by default. Applications that use only the ordinary
TCP/UDP sockets can disable them to reduce library size:

```sh
cmake -S . -B build-core -DCMAKE_BUILD_TYPE=Release -DLIBAEON_ENABLE_EVENTS=OFF
cmake --build build-core --parallel
```

This removes event socket implementations, event polling, callbacks, and event-only
examples from the build. Ordinary socket classes, `socket_set`, and optional
SSL/TLS remain available. Include `<aeon.hpp>` as the public entry point.
The public headers are source-controlled; no generated headers are required.

Aeon CMake targets propagate `LIBAEON_HAS_EVENTS` and `LIBAEON_HAS_SSL`
(as 0 or 1) and `ENABLE_SSL` when TLS is enabled. The static target also
propagates `LIBAEON_STATIC` to disable Windows DLL imports. When linking
without an Aeon CMake target, define the options matching the binary:
`LIBAEON_HAS_EVENTS=0` when events are disabled, `LIBAEON_HAS_SSL=1` or
`ENABLE_SSL` when TLS is enabled, and `LIBAEON_STATIC` for a static library.
Unspecified feature switches default to events on and TLS off.

## Documentation and examples

- [Documentation sources](docs/)
- [C++ examples](example/)
- [GitHub repository](https://github.com/earmbrust/libaeon)

The documentation build uses Doxygen, Breathe and Sphinx. The code examples
and API reference target Aeon 2.0, not the older `net::C*` class names.

## License

BSD 3-Clause. See the project license and copyright notices.
