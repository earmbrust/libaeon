Getting Started
===============

Installation
------------

Windows
~~~~~~~

Using CMake and MSVC::

    mkdir build
    cd build
    cmake .. -G "Visual Studio 17 2022"
    cmake --build . --config Release

Using MinGW::

    mkdir build
    cd build
    cmake .. -G "MinGW Makefiles"
    cmake --build .

Linux
~~~~~

::

    sudo apt-get install cmake build-essential
    mkdir build
    cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release
    cmake --build .

macOS
~~~~~

::

    brew install cmake
    mkdir build
    cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release
    cmake --build .

ARM (Cross-compile)
~~~~~~~~~~~~~~~~~~~

::

    sudo apt-get install cmake gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf
    mkdir build
    cd build
    cmake .. \
      -DCMAKE_SYSTEM_NAME=Linux \
      -DCMAKE_SYSTEM_PROCESSOR=armv7l \
      -DCMAKE_CXX_COMPILER=arm-linux-gnueabihf-g++ \
      -DCMAKE_C_COMPILER=arm-linux-gnueabihf-gcc \
      -DCMAKE_BUILD_TYPE=Release
    cmake --build .

Basic Usage
-----------

Creating a TCP Server
~~~~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

    #include <libaeon.h>
    #include <iostream>

    int main() {
        net::CServerSocket server;
        
        if (!server.Listen(2300)) {
            std::cerr << "Failed to listen on port 2300\n";
            return 1;
        }
        
        std::cout << "Server listening on port 2300\n";
        
        net::CSocket* client = server.Accept();
        if (client && client->connected) {
            std::cout << "Client connected\n";
            client->Write("Hello from server!\n");
            client->Close();
            delete client;
        }
        
        return 0;
    }

Creating a TCP Client
~~~~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

    #include <libaeon.h>
    #include <iostream>

    int main() {
        net::CClientSocket client("localhost", 2300);
        
        if (!client.connected) {
            std::cerr << "Connection failed\n";
            return 1;
        }
        
        char buffer[256];
        int bytes_read = client.Read(buffer, sizeof(buffer) - 1);
        buffer[bytes_read] = '\0';
        
        std::cout << "Server says: " << buffer << std::endl;
        client.Close();
        
        return 0;
    }

UDP Communication
~~~~~~~~~~~~~~~~~

.. code-block:: cpp

    // UDP Server
    net::CServerSocketUDP server;
    server.Listen(2301);
    
    char buffer[256];
    int bytes_read = server.Read(buffer, sizeof(buffer) - 1);
    
    // UDP Client
    net::CClientSocketUDP client("localhost", 2301);
    client.Write("Hello UDP!\n");

Event-Driven I/O
~~~~~~~~~~~~~~~~

.. code-block:: cpp

    #include <libaeon.h>

    class MyEventSocket : public net::CEventSocket {
    public:
        bool OnRead(const char* buffer, int size) override {
            std::cout << "Received " << size << " bytes\n";
            return true; // continue polling
        }
    };

    int main() {
        net::CServerSocket server;
        server.Listen(2300);
        
        net::CEventSocketSet clients;
        
        while (true) {
            net::CEventSocket* client = server.Accept();
            if (client && client->connected) {
                clients.Add(client);
            }
            
            // Poll all connected clients
            clients.Poll();
        }
    }

Next Steps
----------

- Read the :doc:`API Reference <api/index>` for detailed class and method documentation
- Check out the `example programs <https://github.com/earmrust/libaeon/tree/master/example>`_
- Visit the `GitHub repository <https://github.com/earmrust/libaeon>`_ for issues and discussions
