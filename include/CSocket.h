/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#ifndef _CSOCKET_H
#define _CSOCKET_H

#include "CAddress.h"

namespace net {
    // Forward declarations
    class BlockingModeGuard;

    /**
     * \brief RAII guard to temporarily set socket to non-blocking mode
     * \author Elden Armbrust
     *
     * BlockingModeGuard automatically restores the original blocking mode
     * when the guard goes out of scope, even if an error occurs.
     */
    class BlockingModeGuard {
    private:
        class CSocket* socket_;
        bool original_blocking_;
        bool valid_;

    public:
        explicit BlockingModeGuard(class CSocket* socket);
        ~BlockingModeGuard();

        bool IsValid() const { return valid_; }

        // Prevent copying and moving
        BlockingModeGuard(const BlockingModeGuard&) = delete;
        BlockingModeGuard(BlockingModeGuard&&) = delete;
        BlockingModeGuard& operator=(const BlockingModeGuard&) = delete;
        BlockingModeGuard& operator=(BlockingModeGuard&&) = delete;
    };

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
        friend class BlockingModeGuard;
        static const int MaxBufferSize = 256;
        static const int StreamSocketType = SOCK_STREAM;
        static const int DatagramSocketType = SOCK_DGRAM;
        static const int DefaultSocketType = CSocket::StreamSocketType;
        static const int DefaultFamilyType = AF_INET;
        static const int NULLFlag = 0;

        // Public interface
        int SetBlocking(bool flag);
        int SetReadTimeout(int timeout_ms);
        int SetWriteTimeout(int timeout_ms);
        int SetConnectTimeout(int timeout_ms);
        int SetTCPNodelay(bool enabled);
        int SetSOReusAddr(bool enabled);
        int SetSOLinger(u_short linger_time_sec);
        int Write(char* data, int size);
        int Write(const char* data, int size);
        int Write(char* data);
        int Write(const char* data);
        int Write(const std::string& data);
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
        
        // Accessors for remote address information
        std::string GetRemoteIP() const;
        int GetRemotePort() const;
        CAddress GetRemoteAddress() const;
        
        // Error accessors
        int GetError();
        int GetState();
        
        int operator<<(char* data);
        int operator<<(const std::string& data);
        std::string operator>>(std::string);

        // Public members for API compatibility
        socket_t sockfd;
        int n;
        struct sockaddr_storage remote_addr;
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

        void SetError(int error);
        void ClearBuffers();
        void ClearBuffer(char* buffer, int size);
        
        // Socket configuration and utility methods
        void ConfigureSocketForConnect();
        static bool IsValidSocket(socket_t s);
        void SetSocketReusAddr();
        static void SetSocketReusAddr(socket_t sock);
        void SetSocketTCPNodelay();
        void SetSocketLinger(u_short linger_sec);
        
        // Static versions for temporary sockets
        static void SetSocketTCPNodelay(socket_t sock);
        static void SetSocketLinger(socket_t sock, u_short linger_sec);
        
        int SetSocketNonblocking(bool nonblocking);
        void SafeClearBuffer(char* buffer, std::size_t size);
        
        // Internal helpers: Wait for socket I/O with timeout
        static int WaitForReadable(socket_t sockfd, int timeout_ms);
        static int WaitForWritable(socket_t sockfd, int timeout_ms);

#ifdef PLATFORM_WINDOWS
        int wsaret;
        WSADATA wsadata;
#endif
    };
}

#endif // _CSOCKET_H
