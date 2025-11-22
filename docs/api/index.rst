API Reference
=============

Core Classes
------------

.. autodoxygenfile:: libaeon.h
   :project: libaeon

Socket Classes
~~~~~~~~~~~~~~

The libaeon library provides several socket classes for different use cases:

CSocket
^^^^^^^

Base socket class for both TCP and UDP connections.

.. doxygenclass:: net::CSocket
   :members:
   :undoc-members:

CClientSocket
^^^^^^^^^^^^^

TCP client socket for connecting to remote servers.

.. doxygenclass:: net::CClientSocket
   :members:
   :undoc-members:

CServerSocket
^^^^^^^^^^^^^

TCP server socket for accepting client connections.

.. doxygenclass:: net::CServerSocket
   :members:
   :undoc-members:

CEventSocket
^^^^^^^^^^^^

Event-driven socket for polling-based I/O.

.. doxygenclass:: net::CEventSocket
   :members:
   :undoc-members:

UDP Socket Classes
~~~~~~~~~~~~~~~~~~

CSocketUDP
^^^^^^^^^^

Base UDP datagram socket.

.. doxygenclass:: net::CSocketUDP
   :members:
   :undoc-members:

CClientSocketUDP
^^^^^^^^^^^^^^^^

UDP client socket for sending datagrams.

.. doxygenclass:: net::CClientSocketUDP
   :members:
   :undoc-members:

CServerSocketUDP
^^^^^^^^^^^^^^^^

UDP server socket for receiving datagrams.

.. doxygenclass:: net::CServerSocketUDP
   :members:
   :undoc-members:

Socket Set Classes
~~~~~~~~~~~~~~~~~~~

CSocketSet
^^^^^^^^^^

Container for managing multiple CSocket objects.

.. doxygenclass:: net::CSocketSet
   :members:
   :undoc-members:

CEventSocketSet
^^^^^^^^^^^^^^^

Container for managing multiple CEventSocket objects.

.. doxygenclass:: net::CEventSocketSet
   :members:
   :undoc-members:

Library Functions
~~~~~~~~~~~~~~~~~

.. doxygenfunction:: net::GetLibraryVersion

Enumerations and Constants
~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. doxygenenum:: 
   :project: libaeon
