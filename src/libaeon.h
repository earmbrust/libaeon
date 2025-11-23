/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#ifndef _LIBAEON_H
#define _LIBAEON_H
/*!
 \file libaeon.h
 \author Elden Armbrust
 \brief The main libaeon include file.
 libaeon.h is the main include file for both libaeon as well as
 developers looking to develop against libaeon.
 \verbinclude documentation.h
 */

// Platform detection - unified approach
#if defined(_WIN32) || defined(_WIN64)
    #define NOMINMAX
    #define PLATFORM_WINDOWS
    #define WIN32_LEAN_AND_MEAN
    #define _CRT_SECURE_NO_DEPRECATE 1
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <io.h>
    typedef int socklen_t;
#elif defined(__APPLE__)
    #define PLATFORM_MACOS
    #include <sys/socket.h>
    #include <netdb.h>
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <sys/types.h>
    #include <unistd.h>
#elif defined(__linux__)
    #define PLATFORM_LINUX
    #include <sys/socket.h>
    #include <netdb.h>
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <sys/types.h>
    #include <unistd.h>
#else
    // Generic POSIX fallback
    #include <sys/socket.h>
    #include <netdb.h>
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <sys/types.h>
    #include <unistd.h>
#endif

// Multi-platform includes
#include <fcntl.h>
#include <memory>
#include <string>
#include <vector>
#include <cstring>
#include <cstdlib>

// Error and state definitions
#define SOCK_RESOLVE 1
#define SOCK_CREATE 2
#define SOCK_ACCEPT 3
#define SOCK_CONNECT 4
#define SOCK_BIND 5
#define ERR_NONE 0
#define ERR_NOHOST 1
#define ERR_NOSOCKET 2
#define AEON 1

/**
 * \namespace net
 * \author Elden Armbrust
 * \brief The container namespace for all libaeon related classes and methods
 *
 * The net namespace encapsulates all network communications of libaeon
 * to prevent name collision with other implementations.
 */
namespace net {

    // Platform-native socket type - libaeon abstraction
    #ifdef PLATFORM_WINDOWS
        typedef SOCKET socket_t;
        #define INVALID_SOCKET_T ((net::socket_t)(INVALID_SOCKET))
    #else
        typedef int socket_t;
        #define INVALID_SOCKET_T (-1)
    #endif

    // Library-specific error constants
    #define NET_SOCKET_ERROR (-1)

    const char* GetLibraryVersion();

    /**
    * \class CSocket
    * \brief A generic socket class
    * \author Elden Armbrust
    *
    * The CSocket class is a generic socket implementation and may be inherited
    * by custom classes to extend its abilities.
    */
    class CSocket {
    public:
        static const int MaxBufferSize = 256;
        static const int StreamSocketType = SOCK_STREAM;
        static const int DatagramSocketType = SOCK_DGRAM;
        static const int DefaultSocketType = CSocket::StreamSocketType;
        static const int DefaultFamilyType = AF_INET;
        static const int NULLFlag = 0;

        // Public interface - maintained for API compatibility
        int SetBlocking(bool flag);
        int SetReadTimeout(int timeout_ms);
        int SetWriteTimeout(int timeout_ms);
        int SetConnectTimeout(int timeout_ms);
        int SetTCPNodelay(bool enabled);
        int Write(char* data, int size);
        int Write(const char* data, int size);
        int Write(char* data);
        int Write(const char* data);
        int Write(std::string data);
        int Read();
        int Read(char* buffer, int size);
        int ReadLine(char* buffer, int size);
        int ReadUntil(char* buffer, int size);
        std::string Read(int size);
        
        CSocket();
        CSocket(int family_type);
        CSocket(socket_t existing_fd, bool is_existing_socket);
        virtual ~CSocket();
        
        bool Close();
        
        int operator<<(char* data);
        int operator<<(std::string data);
        std::string operator>>(std::string);

        // Public members for API compatibility
        socket_t sockfd;
        int n;
        struct sockaddr_in remote_addr;
        bool connected;

    protected:
        int flags;
        int net_family;
        int token_size;
        bool blocking;
        char inbuffer[CSocket::MaxBufferSize];
        char outbuffer[CSocket::MaxBufferSize];
        std::string remote_host;
        std::string remote_ip;
        int connect_code;
        int error_code;
        int error_state;
        int port;
        int read_timeout_ms;
        int write_timeout_ms;
        int connect_timeout_ms;

        int GetState();
        int GetError();
        void SetError(int error);
        void ClearBuffers();
        void ClearBuffer(char* buffer, int size);
        
        // Internal helpers: Wait for socket I/O with timeout
        // Returns: > 0 if ready, 0 if timeout, < 0 if error
        static int WaitForReadable(socket_t sockfd, int timeout_ms);
        static int WaitForWritable(socket_t sockfd, int timeout_ms);

#ifdef PLATFORM_WINDOWS
        int wsaret;
        WSADATA wsadata;
#endif
    };

    /**
     * \brief A socket class to handle client-based communications.
     * \author Elden Armbrust
     *
     * CClientSocket is a multipurpose TCP socket class designed to handle
     * client system communications. It inherits from CSocket.
     */
    class CClientSocket : public CSocket {
    public:
        bool Connect();
        bool Connect(const char* remote, int port);
        CClientSocket();
        CClientSocket(std::string *remote, int port);
        CClientSocket(const char* remote, int port);
        ~CClientSocket();
    protected:
        struct sockaddr_in serv_addr;
        struct addrinfo *server;
    };

    /**
     * \brief A socket class to handle server-based communications.
     * \author Elden Armbrust
     *
     * CServerSocket is a multipurpose TCP socket class designed to handle
     * server connection handling.
     */
    class CEventSocket;  // Forward declaration - defined below
    
    class CServerSocket : public CSocket {
    public:
        CServerSocket();
        ~CServerSocket();
        bool Listen();
        bool Listen(int port);
        std::unique_ptr<CEventSocket> Accept();
        std::unique_ptr<CEventSocket> Accept(bool blocking);
        CEventSocket* Accept(CEventSocket* client_socket, bool blocking = false);
        int SetAcceptTimeout(int timeout_ms);
        int accept_timeout_ms;
    protected:
        struct sockaddr_in serv_addr;
        struct hostent *server;
        socket_t server_socket;
    };

    /**
      * \brief Inherited class allowing for polled updates.
      * \author Elden Armbrust
      *
      * CEventSocket is the base class for event-driven socket classes.
      * CEventSocket can be used to poll for data on the connection,
      * and when data arrives will call the OnRead() member function.
      * \note CEventSocket should be inherited and OnRead()/OnWrite()
      * member functions should be overridden.
      */
    class CEventSocket : public CSocket {
    public:
        CEventSocket() {}
        explicit CEventSocket(socket_t existing_fd) : CSocket(existing_fd, true) {}
        virtual bool OnRead(const char* buffer, int size);
        virtual void OnWrite(const char* buffer, int size, int sentsize);
        int Write(char* data);
        int Write(const char* data);
        int Write(std::string data);
        bool Poll();
    };

    /**
      * \brief Inherited client class allowing for polled updates.
      * \author Elden Armbrust
      *
      * CEventClientSocket is the event-driven equivalent to CClientSocket.
      */
    class CEventClientSocket : public CClientSocket {
    };

    /**
      * \brief Inherited server class allowing for polled updates.
      * \author Elden Armbrust
      *
      * CEventServerSocket is the event-driven equivalent to CServerSocket.
      */
    class CEventServerSocket : public CServerSocket, public CEventSocket {
    };

    /**
      * \brief A class to encapsulate multiple CSocket objects
      * \author Elden Armbrust
      *
      * CSocketSet is a CSocket container class for handling sockets
      * in an ordered fashion.
      */
    class CSocketSet {
    public:
        std::vector<CSocket*> Sockets;
        bool Add(CSocket* socket_ref);
        bool Add();
        bool Remove(unsigned int index);
        bool Remove(unsigned int index, unsigned int count);
        int Size();
    };

    /**
      * \brief A class to encapsulate multiple CEventSocket objects
      * \author Elden Armbrust
      *
      * CEventSocketSet is a CEventSocket container class for handling
      * sockets in an ordered fashion.
      */
    class CEventSocketSet {
    public:
        std::vector<CEventSocket*> Sockets;
        bool Add(CEventSocket* socket_ref);
        bool Add();
        bool Remove(unsigned int index);
        bool Remove(unsigned int index, unsigned int count);
        int Size();
        void Poll();
        void Cleanup();
    };

    /**
     * \brief UDP socket class
     * \author Elden Armbrust
     *
     * CSocketUDP is a UDP datagram socket implementation.
     */
    class CSocketUDP : public CSocket {
    public:
        CSocketUDP();
        int Write(char* data, int size);
        int Write(const char* data, int size);
        int Write(char* data);
        int Write(const char* data);
        int Write(std::string data);
        int Read();
        int Read(char* buffer, int size);
        int ReadUntil(char* buffer, int size);
        std::string Read(int size);
        static const int DefaultSocketType = SOCK_DGRAM;
    };

    /**
     * \brief A UDP socket class to handle server-based communications.
     * \author Elden Armbrust
     *
     * CServerSocketUDP is a UDP socket class designed to handle
     * server datagram reception.
     */
    class CServerSocketUDP : public CSocketUDP {
    public:
        CServerSocketUDP();
        ~CServerSocketUDP();
        bool Listen();
        bool Listen(int port);
    protected:
        struct sockaddr_in serv_addr;
    };

    /**
     * \brief A UDP socket class to handle client-based communications.
     * \author Elden Armbrust
     *
     * CClientSocketUDP is a UDP socket class designed to handle
     * client datagram transmission.
     */
    class CClientSocketUDP : public CSocketUDP {
    public:
        CClientSocketUDP();
        CClientSocketUDP(const char* hostname, int port);
        CClientSocketUDP(std::string* hostname, int port);
        ~CClientSocketUDP();
        bool Connect();
        bool Connect(const char* hostname, int port);
    protected:
        struct sockaddr_in serv_addr;
        struct addrinfo* server;
    };

}  // namespace net

#endif // _LIBAEON_H