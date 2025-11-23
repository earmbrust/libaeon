# libAeon

**libAeon** is a cross-platform C++ networking library providing straightforward TCP and UDP socket management with both blocking and event-driven modes. It's designed to be lightweight, simple to use, and suitable for applications ranging from high-performance servers to embedded systems.

## Design Goals

- **Lightweight & Efficient**: Minimal memory overhead with straightforward socket management
- **Cross-Platform**: Native support for Windows, Linux, and macOS
- **Reliability-Focused**: Built for stable, production network communications
- **Simple API**: Intuitive C++ interface with the `net` namespace
- **Flexible Architecture**: Support for TCP, UDP, blocking, and event-driven sockets
- **No External Dependencies**: Uses only standard C++ and platform socket APIs
- **Small Codebase**: Suitable for embedded systems and resource-constrained environments

## Supported Platforms

- **Linux** (GCC/Clang)
- **Windows** (MSVC, MinGW)
- **macOS** (Clang)
- **ARM/Cross-compilation** (ARMv7 and above)

## Quick Start

### Building

libAeon uses CMake for cross-platform builds:

```bash
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
sudo cmake --install .
```

For specific platforms or architectures:

```bash
# Windows MSVC
cmake .. -G "Visual Studio 17 2022" -DCMAKE_BUILD_TYPE=Release

# ARM cross-compilation
cmake .. \
  -DCMAKE_SYSTEM_NAME=Linux \
  -DCMAKE_SYSTEM_PROCESSOR=armv7l \
  -DCMAKE_CXX_COMPILER=arm-linux-gnueabihf-g++ \
  -DCMAKE_C_COMPILER=arm-linux-gnueabihf-gcc
```

### Basic Usage

Create a simple client connection:

```cpp
#include <libaeon.h>

net::CClientSocket client("example.com", 8080);
if (client.connected) {
    client.Write("GET / HTTP/1.1\r\nHost: example.com\r\n\r\n");
    std::string response = client.Read(1024);
}
```

Create a server and accept connections:

```cpp
#include <libaeon.h>

net::CServerSocket server;
if (server.Listen(8080)) {
    net::CEventSocket* client = server.Accept();
    if (client && client->connected) {
        std::string data = client->Read(256);
        client->Write("Hello from server!");
        delete client;
    }
}
```

UDP datagram communication:

```cpp
net::CServerSocketUDP udp_server;
udp_server.Listen(5000);

char buffer[256];
int bytes = udp_server.Read(buffer, sizeof(buffer));
if (bytes > 0) {
    udp_server.Write(buffer, bytes);  // Echo back
}
```

## Architecture

libAeon provides multiple socket classes for different use cases:

- **CSocket**: Base socket class with core functionality
- **CClientSocket**: TCP client connections
- **CServerSocket**: TCP server socket accepting multiple connections
- **CEventSocket**: Non-blocking, event-driven socket for high-performance polling
- **CEventSocketSet**: Container for managing multiple event sockets
- **CSocketUDP**: UDP datagram socket base class
- **CClientSocketUDP**: UDP client for sending datagrams
- **CServerSocketUDP**: UDP server for receiving datagrams

All classes are in the `net` namespace:

```cpp
net::CSocket connection;
net::CServerSocket server;
```

## Usage Patterns

- Event-driven sockets (`CEventSocket`) support high-concurrency polling scenarios
- UDP sockets are useful for low-latency, connectionless communication
- Blocking mode works well for simple request-response patterns
- Non-blocking mode can prevent thread stalls in concurrent applications

## Examples

See the `example/` directory for complete working examples:

- `http-client.cpp` - HTTP client implementation
- `simple-server.cpp` - Basic TCP server
- `udp-server.cpp` / `udp-client.cpp` - UDP datagram examples
- `test-client.cpp` - Extended client examples

## Cross-Compilation

For embedded systems and ARM targets:

```bash
cmake .. \
  -DCMAKE_SYSTEM_NAME=Linux \
  -DCMAKE_SYSTEM_PROCESSOR=armv7l \
  -DCMAKE_CXX_COMPILER=arm-linux-gnueabihf-g++ \
  -DCMAKE_C_COMPILER=arm-linux-gnueabihf-gcc \
  -DARM_ARCH=armv7-a
```

## Version

libAeon uses semantic versioning. The version is derived from git tags (format: `MAJOR.MINOR.PATCH`) or development branch names.

## License

libAeon is distributed under the [BSD 3-Clause License](https://opensource.org/licenses/BSD-3-Clause).

```
Copyright (c) 2006-2025, Elden Armbrust
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the
      distribution.
    * Neither the name of libaeon nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```

## Contributing

libAeon is maintained by Elden Armbrust. Contributions and feedback are welcome.

## See Also

- Doxygen API documentation: Available in `docs/`
- CMake build configuration: `CMakeLists.txt`
- Examples: `example/` directory