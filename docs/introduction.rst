Introduction
============

What is Aeon?
-------------

Aeon (library file ``libaeon``) is a lightweight C++17 networking library.
It provides socket abstractions for applications using TCP, UDP and optional
SSL/TLS on Windows, Linux and macOS. Its public C++ namespace is ``aeon``.

Design
------

The public API follows the C++ Core Guidelines' preferred ``underscore_style``:
``aeon::socket``, ``aeon::client_socket``, ``aeon::server_socket``, and
``aeon::udp_socket``. The generic ``aeon::socket`` class remains extensible.
Class names do not use a ``C`` prefix; the old ``net::CSocket`` naming belongs
to earlier API versions and is not part of the 2.0 interface.

Aeon uses native socket implementations internally, with a common C++ surface
and deterministic descriptor cleanup. Native socket descriptors have exclusive
ownership and socket objects cannot be copied.

Features
--------

- **TCP:** client and server connections, blocking and non-blocking I/O
- **UDP:** datagrams, including configurable receive timeouts
- **Events:** callbacks and socket collections for polling-based I/O
- **Addresses:** IPv4/IPv6 addresses and resolution
- **TLS:** optional OpenSSL-backed client sockets when enabled at build time

Supported platforms
-------------------

GitHub Actions builds and tests supported Windows, Linux and macOS configurations,
including native x64/ARM64 and selected cross-compiled targets. Consult the
`GitHub Actions status <https://github.com/earmbrust/libaeon/actions>`_ for the
latest verified results.

See :doc:`Getting Started <getting_started>` for build commands and examples,
or browse the :doc:`API Reference <api/index>`.
