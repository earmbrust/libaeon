Getting Started
===============

Aeon 2.0 requires C++20 and CMake 3.24 or newer. Public types and functions use the ``aeon``
namespace and lowercase ``snake_case`` names.

Build and test
--------------

Linux / macOS
~~~~~~~~~~~~~

From the repository root::

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --parallel

Install a C++ compiler and CMake first (GCC or Clang on Linux, Xcode Command
Line Tools on macOS). Tests are disabled by default. To build and run them::

    cmake -S . -B build-tests -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
    cmake --build build-tests --parallel
    ctest --test-dir build-tests --output-on-failure

Google Test is fetched only when testing is enabled. Examples and benchmarks
are separately opt-in with ``LIBAEON_BUILD_EXAMPLES=ON`` and
``LIBAEON_BUILD_BENCHMARKS=ON``.

Windows (MSVC)
~~~~~~~~~~~~~~

Using Visual Studio 2022::

    cmake -S . -B build-vs2022 -G "Visual Studio 17 2022" -A x64
    cmake --build build-vs2022 --config Release

Using Visual Studio 2026 (requires CMake with VS 2026 generator support)::

    cmake -S . -B build-vs2026 -G "Visual Studio 18 2026" -A x64
    cmake --build build-vs2026 --config Release

To enable and run tests on Visual Studio 2026, use a separate build directory::

    cmake -S . -B build-vs2026-tests -G "Visual Studio 18 2026" -A x64 -DBUILD_TESTING=ON
    cmake --build build-vs2026-tests --config Release
    ctest --test-dir build-vs2026-tests -C Release --output-on-failure

Installable CMake package
-------------------------

Build and install into a chosen prefix::

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
    cmake --build build --config Release
    cmake --install build --config Release --prefix ./dist/libaeon

Use a separate prefix for each platform, toolchain and feature configuration.
A downstream CMake project can consume either installed library variant::

    find_package(libaeon 2.0 CONFIG REQUIRED)
    target_link_libraries(my_program PRIVATE aeon::aeon)

The static alternative is ``aeon::aeon-static``. Supply the installation
prefix through ``CMAKE_PREFIX_PATH``. Installed targets provide their feature
macros and link requirements, including OpenSSL when enabled. Shared-library
applications also require the installed runtime library to be discoverable by
the operating system's dynamic loader. On Windows, its DLL is in ``bin``.

A pull request builds a documentation preview artifact but does not deploy to
GitHub Pages. Publication is reserved for eligible master or version-tag
pushes, and requires the repository's Pages deployment configuration.

Optional event sockets
~~~~~~~~~~~~~~~~~~~~~~

Event-driven sockets are enabled by default. If your application uses only
ordinary TCP and UDP sockets, configure with
``-DLIBAEON_ENABLE_EVENTS=OFF`` to exclude event handling and polling code.
This option is independent of SSL/TLS. ``socket_set`` and server-owned peers
remain available in either configuration. Include ``<aeon.hpp>`` in your
application. The ``aeon`` and ``aeon-static`` CMake targets propagate the
feature definitions and required include paths to consumers.

Event callbacks
~~~~~~~~~~~~~~~

``event_socket_set`` registers event sockets without taking ownership. The
application must remove registered sockets before destroying them and must not
destroy a socket during active event dispatch. ``on_socket_error`` registers a
TCP socket error callback; ``on_udp_socket_error`` registers the corresponding
UDP error callback. An empty callback clears the registration and restores the
virtual ``on_*_impl`` handler. Callbacks run in the thread that polls the set.

Optional SSL/TLS
~~~~~~~~~~~~~~~~

Install OpenSSL development libraries for the target architecture and configure
with ``-DENABLE_SSL=ON``. For example::

    cmake -S . -B build-ssl -DCMAKE_BUILD_TYPE=Release -DENABLE_SSL=ON -DBUILD_TESTING=ON
    cmake --build build-ssl --parallel
    ctest --test-dir build-ssl --output-on-failure

On Windows, an architecture-matched OpenSSL package (for example, from vcpkg)
is required. SSL/TLS socket classes are conditional on ``ENABLE_SSL``.

``ssl_client_socket`` defaults to **strict verification**: certificate trust
and the requested DNS name or IP address must both match. The client sends SNI
for DNS names. Calling ``connect(const address&)`` verifies that numeric IP.
Use ``set_ca_file()`` to load a private CA. ``set_allow_self_signed(true)``
explicitly allows a self-signed *leaf* certificate in strict mode but does not
bypass identity, validity dates or handshake failures. The safer default is
``false``. ``set_peer_verification(ssl_verify_mode::off)`` disables validation
and must only be used when this risk is understood.

The ``http-client`` example now verifies HTTPS by default. Its ``--no-verify``
flag explicitly disables this protection. For local TLS regression tests,
build with ``ENABLE_SSL=ON`` and ``BUILD_TESTING=ON``; CTest includes real
loopback handshakes for the shared and static targets.

TCP server
----------

``server_socket::accept()`` returns a borrowed ``aeon::socket*``. The
server owns accepted peers and closes them when the listener closes.
``server_socket::remove_peer()`` retires a peer; borrowed pointers must not be
used after the peer is removed or the server is destroyed.

.. code-block:: cpp

    #include <aeon.hpp>
    #include <iostream>

    int main() {
        aeon::server_socket server;
        if (!server.listen("127.0.0.1", 2300)) {
            std::cerr << "Could not listen\n";
            return 1;
        }

        auto client = server.accept();
        if (client && client->connected) {
            client->write("Hello from Aeon!\n");
            client->close();
            server.remove_peer(client); // Release the listener-owned peer
        }
    }

TCP client
----------

.. code-block:: cpp

    #include <aeon.hpp>
    #include <iostream>

    int main() {
        aeon::client_socket client;
        if (!client.connect("127.0.0.1", 2300)) {
            std::cerr << "Connection failed\n";
            return 1;
        }

        char buffer[256]{};
        const int count = client.read(buffer, sizeof(buffer));
        if (count > 0) {
            std::cout.write(buffer, count);
        }
        client.close();
    }

UDP server
----------

UDP is connectionless. A zero-length datagram is valid and is not a connection
closure. With a configured timeout, ``read`` returns 0 if no datagram arrives.

.. code-block:: cpp

    #include <aeon.hpp>

    int main() {
        aeon::udp_server_socket server;
        if (!server.listen(2301)) {
            return 1;
        }
        server.set_read_timeout(1000); // milliseconds

        char buffer[256]{};
        const int count = server.read(buffer, sizeof(buffer));
        if (count > 0) {
            if (auto* peer = server.get_peer(server.get_remote_address())) {
                peer->write(buffer, count); // Reply through the server-owned peer
            }
        }
    }

Extending the socket abstraction
--------------------------------

``aeon::socket`` remains a public extensible base class. Overriding a virtual
I/O method allows applications to customize socket behavior:

.. code-block:: cpp

    #include <aeon.hpp>

    class custom_socket : public aeon::socket {
    public:
        int read(char* buffer, int size) override {
            return aeon::socket::read(buffer, size);
        }
    };

See the :doc:`API Reference <api/index>` and the
`source examples <https://github.com/earmbrust/libaeon/tree/2.0-devel/example>`_
for additional usage patterns.
