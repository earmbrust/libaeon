API Reference
=============

Aeon 2.0 uses the ``aeon`` namespace and lowercase ``snake_case`` names for
classes and functions, following the C++ Core Guidelines' preferred style.
Include ``<aeon.hpp>`` to use the complete public API.

Core sockets
------------

``aeon::socket`` is the extensible base abstraction shared by the TCP and UDP
socket families. It owns its native socket handle; it cannot be copied.

.. doxygenclass:: aeon::socket
   :members:

.. doxygenclass:: aeon::blocking_mode_guard
   :members:

TCP sockets
-----------

.. doxygenclass:: aeon::client_socket
   :members:

.. doxygenclass:: aeon::server_socket
   :members:

UDP sockets
-----------

.. doxygenclass:: aeon::udp_socket
   :members:

.. doxygenclass:: aeon::udp_client_socket
   :members:

.. doxygenclass:: aeon::udp_server_socket
   :members:

Event-driven sockets
--------------------

.. doxygenclass:: aeon::event_socket
   :members:

.. doxygenclass:: aeon::client_event_socket
   :members:

.. doxygenclass:: aeon::server_event_socket
   :members:

Socket sets
-----------

.. doxygenclass:: aeon::socket_set
   :members:

.. doxygenclass:: aeon::event_socket_set
   :members:

Addresses and resolution
------------------------

.. doxygenclass:: aeon::address
   :members:

.. doxygenclass:: aeon::resolver
   :members:

Optional SSL/TLS support
------------------------

These classes are available when the project is built with
``-DENABLE_SSL=ON`` and OpenSSL is available. The documentation includes the
optional API even when the documentation build itself does not link OpenSSL.

.. doxygenclass:: aeon::ssl_socket
   :members:

.. doxygenclass:: aeon::ssl_client_socket
   :members:

.. doxygenenum:: aeon::ssl_verify_mode

Library version
---------------

.. doxygenfunction:: aeon::get_library_version
