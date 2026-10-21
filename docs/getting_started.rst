Getting Started
===============

Aeon 2.0 requires C++17 and CMake. Public types and functions use the ``aeon``
namespace and lowercase ``snake_case`` names.

Build and test
--------------

Linux / macOS
~~~~~~~~~~~~~

From the repository root::

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --parallel
    ctest --test-dir build --output-on-failure

Install a C++ compiler and CMake first (GCC or Clang on Linux, Xcode Command
Line Tools on macOS). CMake fetches Google Test when configuring tests.

Windows (MSVC)
~~~~~~~~~~~~~~

Using Visual Studio 2022::

    cmake -S . -B build-vs2022 -G "Visual Studio 17 2022" -A x64
    cmake --build build-vs2022 --config Release
    ctest --test-dir build-vs2022 -C Release --output-on-failure

Using Visual Studio 2026 (requires CMake with VS 2026 generator support)::

    cmake -S . -B build-vs2026 -G "Visual Studio 18 2026" -A x64
    cmake --build build-vs2026 --config Release
    ctest --test-dir build-vs2026 -C Release --output-on-failure

Optional SSL/TLS
~~~~~~~~~~~~~~~~

Install OpenSSL development libraries for the target architecture and configure
with ``-DENABLE_SSL=ON``. For example::

    cmake -S . -B build-ssl -DCMAKE_BUILD_TYPE=Release -DENABLE_SSL=ON
    cmake --build build-ssl --parallel
    ctest --test-dir build-ssl --output-on-failure

On Windows, an architecture-matched OpenSSL package (for example, from vcpkg)
is required. SSL/TLS socket classes are conditional on ``ENABLE_SSL``.

TCP server
----------

``server_socket::accept()`` returns a ``std::unique_ptr<aeon::event_socket>``.
The object is destroyed automatically when its owning pointer leaves scope.

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
            server.write(buffer, count); // Reply to last sender
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
