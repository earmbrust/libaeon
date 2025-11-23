Introduction
============

What is libaeon?
----------------

libaeon is a lightweight, cross-platform C++ networking library that provides simple abstractions for socket programming. Whether you're building a networked game, a distributed system, or a simple client-server application, libaeon makes it easy to work with TCP and UDP sockets across Windows, Linux, macOS, and ARM platforms.

Design Philosophy
------------------

libaeon follows these core principles:

1. **Simplicity**: The API is straightforward and intuitive. Common tasks should require minimal code.
2. **Portability**: Write once, compile everywhere. The same code works on Windows, Linux, macOS, and ARM.
3. **Lightweight**: Minimal dependencies and overhead. libaeon doesn't force you to use heavy frameworks.
4. **Flexibility**: Support for both synchronous blocking I/O and event-driven non-blocking I/O patterns.

Key Features
------------

**TCP Networking**
  - Client and server socket classes
  - Blocking and non-blocking modes
  - Configurable timeouts for read, write, and connect operations

**UDP Networking**
  - UDP datagram sockets for both client and server
  - Full support for UDP-specific operations

**Event-Driven I/O**
  - Poll-based event handling with callback methods
  - Socket sets for managing multiple connections efficiently
  - Perfect for servers handling many concurrent clients

**Cross-Platform Support**
  - Unified API across Windows, Linux, macOS, and ARM
  - Automatic platform detection and abstraction
  - Same code compiles everywhere

Platform Support
----------------

- **Windows**: Visual Studio (MSVC), MinGW, Clang
- **Linux**: GCC, Clang (x86_64, ARM)
- **macOS**: Apple Clang (x86_64, ARM64)
- **ARM**: ARMv7, ARMv8 (32-bit and 64-bit)

C++ Standard
------------

libaeon requires C++17 or later. If you're using an older compiler, please upgrade to benefit from modern C++ features and better standard library support.

License
-------

libaeon is released under the BSD License, which permits both commercial and personal use with minimal restrictions. See the LICENSE file in the repository for full details.

Contributing
------------

libaeon is open source and welcomes contributions. If you find a bug, have a feature request, or want to submit a pull request, please visit the `GitHub repository <https://github.com/earmrust/libaeon>`_.

Getting Help
------------

- Check the :doc:`Getting Started <getting-started.rst>` guide
- Review the :doc:`API Reference <api/index>`
- Look at the `example programs <https://github.com/earmrust/libaeon/tree/master/example>`_
- Open an issue on GitHub for bugs or feature requests
