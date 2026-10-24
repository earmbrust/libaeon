Aeon Documentation
==================

Aeon is a lightweight, cross-platform C++20 networking library. The compiled
library is named ``libaeon`` on platforms that use that filename convention;
its C++ namespace is ``aeon``.

.. toctree::
   :maxdepth: 2
   :caption: Contents:

   introduction
   getting_started
   api/index

Features
--------

- TCP client/server and UDP datagram sockets
- IPv4 and IPv6 addressing and DNS resolution
- Extensible socket and event-driven abstractions
- Configurable socket timeouts
- Optional OpenSSL-based TLS support
- Windows, Linux, macOS, x86, x64, ARMv7 and ARM64 build coverage

The :doc:`API Reference <api/index>` documents the current 2.0 C++ interface.
Browse the `source repository <https://github.com/earmbrust/libaeon>`_ for
examples and development status.
