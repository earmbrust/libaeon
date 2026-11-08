# Aeon (`libaeon`)

Aeon is a lightweight, cross-platform **C++20** networking library for TCP,
UDP, socket events, IPv4/IPv6 addressing and optional OpenSSL/TLS support.
**`aeon` is the C++ namespace; `libaeon` is the library file name.**

## Using Aeon

The public API uses the `aeon` namespace and lowercase `snake_case` names.
Include `<aeon.hpp>` for the public socket classes.

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

For TCP servers, UDP sockets and SSL/TLS examples, see the
[Getting Started guide](docs/getting_started.rst). The
[socket configuration reference](QUICK_REFERENCE.md) describes timeouts,
read/write results and socket ownership.
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

## Installing for CMake consumers

Use a separate installation prefix for each compiler, architecture and feature
configuration. Build and install both library variants:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build --config Release
cmake --install build --config Release --prefix ./dist/libaeon
```

A separate CMake project can then use the installed package:

```cmake
find_package(libaeon 2.0 CONFIG REQUIRED)
target_link_libraries(my_program PRIVATE aeon::aeon)
# Or link aeon::aeon-static for the static library.
```

Pass `-DCMAKE_PREFIX_PATH=<installation-prefix>` to the consumer's CMake
configuration. Installed targets propagate C++20, feature macros and required
system libraries. TLS-enabled packages find OpenSSL through CMake. When using
the shared library, the dynamic loader must find its DLL, .so or .dylib at run
time; on Windows, the DLL is installed in `bin/`.


## Documentation and examples

- [Getting Started](docs/getting_started.rst)
- [API reference](docs/api/index.rst)
- [Socket configuration reference](QUICK_REFERENCE.md)
- [Examples](example/)
- [ChangeLog](ChangeLog)
- [Stable release](https://github.com/earmbrust/libaeon/releases/tag/v2.0.1)

## License

BSD 3-Clause. See the project license and copyright notices.

libAeon is authored and maintained by Elden Armbrust
(<eldenarmbrust@gmail.com>).
