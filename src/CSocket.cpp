/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#ifndef _CSOCKET_CPP
#define _CSOCKET_CPP

#include "libaeon.h"
#include <cstring>
#include <algorithm>

namespace net {




/**
 * BlockingModeGuard implementation
 * Manages socket blocking mode with RAII semantics
 */
BlockingModeGuard::BlockingModeGuard(CSocket* socket) 
    : socket_(socket), original_blocking_(false), valid_(false) {
    
    if (!socket_) {
        return;
    }
    
    original_blocking_ = socket_->blocking;
    
    // Attempt to set non-blocking mode
    if (socket_->SetBlocking(false) == 0) {
        // Success - mark guard as valid so destructor will restore
        valid_ = true;
    }
    // If SetBlocking failed, valid_ remains false and destructor won't restore
}

BlockingModeGuard::~BlockingModeGuard() {
    // Only restore if we successfully entered non-blocking mode
    if (valid_ && socket_) {
        // Ignore restoration errors - we tried our best
        socket_->SetBlocking(original_blocking_);
    }
}

/**
 * Configure socket for optimal connection performance
 * Sets SO_REUSEADDR and TCP_NODELAY
 */
void CSocket::ConfigureSocketForConnect() {
    this->SetSocketReusAddr();
    this->SetSocketTCPNodelay();
}

/**
 * Check if socket file descriptor is valid (static)
 */
bool CSocket::IsValidSocket(socket_t s) {
#ifdef PLATFORM_WINDOWS
    return s != INVALID_SOCKET;
#else
    return s >= 0;
#endif
}

/**
 * Set SO_REUSEADDR socket option
 */
void CSocket::SetSocketReusAddr() {
    int reuse = 1;
    setsockopt(this->sockfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));
}

/**
 * Set SO_REUSEADDR socket option (static version)
 */
void CSocket::SetSocketReusAddr(socket_t sock) {
    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));
}

/**
 * Disable Nagle's algorithm with TCP_NODELAY
 */
void CSocket::SetSocketTCPNodelay() {
    int nodelay = 1;
    setsockopt(this->sockfd, IPPROTO_TCP, TCP_NODELAY, (const char*)&nodelay, sizeof(nodelay));
}

/**
 * Disable Nagle's algorithm with TCP_NODELAY (static version)
 */
void CSocket::SetSocketTCPNodelay(socket_t sock) {
    int nodelay = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&nodelay, sizeof(nodelay));
}

/**
 * Set socket linger option
 */
void CSocket::SetSocketLinger(u_short linger_sec) {
    struct linger linger_opt = {0, 0};
    linger_opt.l_onoff = (linger_sec > 0) ? 1 : 0;
    linger_opt.l_linger = linger_sec;
    setsockopt(this->sockfd, SOL_SOCKET, SO_LINGER, (const char*)&linger_opt, sizeof(linger_opt));
}

/**
 * Set socket linger option (static version)
 */
void CSocket::SetSocketLinger(socket_t sock, u_short linger_sec) {
    struct linger linger_opt = {0, 0};
    linger_opt.l_onoff = (linger_sec > 0) ? 1 : 0;
    linger_opt.l_linger = linger_sec;
    setsockopt(sock, SOL_SOCKET, SO_LINGER, (const char*)&linger_opt, sizeof(linger_opt));
}

/**
 * Set socket to non-blocking mode
 */
int CSocket::SetSocketNonblocking(bool nonblocking) {
#ifdef PLATFORM_WINDOWS
    u_long mode = nonblocking ? 1 : 0;
    return ioctlsocket(this->sockfd, FIONBIO, &mode);
#else
    int flags = fcntl(this->sockfd, F_GETFL, 0);
    if (flags < 0) return -1;
    
    if (nonblocking) {
        return fcntl(this->sockfd, F_SETFL, flags | O_NONBLOCK);
    } else {
        return fcntl(this->sockfd, F_SETFL, flags);
    }
#endif
}

/**
 * Safely clear a buffer
 */
void CSocket::SafeClearBuffer(char* buffer, std::size_t size) {
    if (buffer && size > 0) {
        std::memset(buffer, 0, size);
    }
}

// Helper: Wait for socket to be readable with timeout
int CSocket::WaitForReadable(socket_t sockfd, int timeout_ms) {
    if (timeout_ms <= 0) {
        return 1;  // No timeout, proceed immediately
    }

#ifdef PLATFORM_WINDOWS
    fd_set readset;
    FD_ZERO(&readset);
    FD_SET(sockfd, &readset);
    
    timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    
    int select_result = select(0, &readset, nullptr, nullptr, &tv);
#else
    fd_set readset;
    FD_ZERO(&readset);
    FD_SET(sockfd, &readset);
    
    timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    
    int select_result = select(sockfd + 1, &readset, nullptr, nullptr, &tv);
#endif
    
    return select_result;
}

// Helper: Wait for socket to be writable with timeout
int CSocket::WaitForWritable(socket_t sockfd, int timeout_ms) {
    if (timeout_ms <= 0) {
        return 1;  // No timeout, proceed immediately
    }

#ifdef PLATFORM_WINDOWS
    fd_set writeset;
    FD_ZERO(&writeset);
    FD_SET(sockfd, &writeset);
    
    timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    
    int select_result = select(0, nullptr, &writeset, nullptr, &tv);
#else
    fd_set writeset;
    FD_ZERO(&writeset);
    FD_SET(sockfd, &writeset);
    
    timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    
    int select_result = select(sockfd + 1, nullptr, &writeset, nullptr, &tv);
#endif
    
    return select_result;
}

/**
 * CSocket operator<< for char* data
 * \param data Pointer to null-terminated string
 * \return Number of bytes written to socket
 */
int CSocket::operator<<(char* data) {
    if (!data) {
        return NET_SOCKET_ERROR;
    }
    return this->Write(data);
}

/**
 * CSocket operator<< for std::string data
 * \param data std::string object to write
 * \return Number of bytes written to socket
 */
int CSocket::operator<<(const std::string& data) {
    return this->Write(data);
}

/**
 * CSocket operator>> for reading data
 * \param (unused parameter for operator syntax)
 * \return std::string containing read data
 */
std::string CSocket::operator>>(std::string) {
    return this->Read(CSocket::MaxBufferSize);
}

/**
 * GetState retrieves the current state of the CSocket
 * \return The current state of the socket
 */
int CSocket::GetState() {
    return this->error_state;
}

/**
 * GetError retrieves the current error code in the CSocket object
 * \return The error code currently stored
 */
int CSocket::GetError() {
    return this->error_code;
}

/**
 * SetError sets the current error code in the CSocket object
 * \param error The error code to set
 */
void CSocket::SetError(int error) {
    this->error_code = error;
}

/**
 * GetRemoteIP returns the remote address as a human-readable string
 * \return IP address string (IPv4 or IPv6), or empty string if not set
 */
std::string CSocket::GetRemoteIP() const {
    char ip_str[INET6_ADDRSTRLEN] = {0};
    
    if (remote_addr.ss_family == AF_INET) {
        // Regular IPv4
        struct sockaddr_in* addr4 = (struct sockaddr_in*)&remote_addr;
        if (inet_ntop(AF_INET, &addr4->sin_addr, ip_str, sizeof(ip_str))) {
            return std::string(ip_str);
        }
    } else if (remote_addr.ss_family == AF_INET6) {
        struct sockaddr_in6* addr6 = (struct sockaddr_in6*)&remote_addr;
        
        // Check if this is an IPv4-mapped IPv6 address (::ffff:a.b.c.d)
        // IPv4-mapped addresses have first 80 bits as 0, next 16 bits as 0xffff
        if (IN6_IS_ADDR_V4MAPPED(&addr6->sin6_addr)) {
            // Extract the IPv4 address from the last 32 bits
            struct in_addr v4addr;
            std::memcpy(&v4addr, addr6->sin6_addr.s6_addr + 12, sizeof(v4addr));
            if (inet_ntop(AF_INET, &v4addr, ip_str, sizeof(ip_str))) {
                return std::string(ip_str);
            }
        }
        
        // Regular IPv6
        if (inet_ntop(AF_INET6, &addr6->sin6_addr, ip_str, sizeof(ip_str))) {
            return std::string(ip_str);
        }
    }
    
    return std::string();
}

/**
 * GetRemotePort returns the remote port number
 * \return Port number (0-65535), or 0 if not set
 */
int CSocket::GetRemotePort() const {
    if (remote_addr.ss_family == AF_INET) {
        // IPv4
        struct sockaddr_in* addr4 = (struct sockaddr_in*)&remote_addr;
        return ntohs(addr4->sin_port);
    } else if (remote_addr.ss_family == AF_INET6) {
        // IPv6
        struct sockaddr_in6* addr6 = (struct sockaddr_in6*)&remote_addr;
        return ntohs(addr6->sin6_port);
    }
    
    return 0;
}

/**
 * GetRemoteAddress returns the remote address as a CAddress object
 * \return CAddress wrapping the remote peer's address and port
 */
CAddress CSocket::GetRemoteAddress() const {
    return CAddress(remote_addr);
}

/**
 * CSocket constructor - initializes socket with default settings
 */
CSocket::CSocket() {
    this->net_family = CSocket::DefaultFamilyType;
    this->connected = false;
    this->blocking = true;
    this->n = 0;
    this->port = 0;
    this->error_code = ERR_NONE;
    this->error_state = 0;
    this->token_size = 0;
    this->read_timeout_ms = 0;  // No timeout by default
    this->write_timeout_ms = 0;  // No timeout by default
    this->connect_timeout_ms = 0;  // No timeout by default

    // Create socket
    this->sockfd = socket(CSocket::DefaultFamilyType, 
                         CSocket::DefaultSocketType, 0);
    
    if (!IsValidSocket(this->sockfd)) {
        this->error_code = ERR_NOSOCKET;
        this->error_state = SOCK_CREATE;
        return;
    }

#if defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS)
    this->flags = fcntl(this->sockfd, F_GETFL, 0);
    if (this->flags < 0) {
        this->error_code = ERR_NOSOCKET;
        this->error_state = SOCK_CREATE;
    }
#endif

    // Clear buffers
    this->SafeClearBuffer(inbuffer, CSocket::MaxBufferSize);
    this->SafeClearBuffer(outbuffer, CSocket::MaxBufferSize);
}

/**
 * CSocket constructor with family type
 * \param family_type The address family type (AF_INET, AF_INET6, etc.)
 */
CSocket::CSocket(int family_type) {
    this->net_family = family_type;
    this->connected = false;
    this->blocking = true;
    this->n = 0;
    this->port = 0;
    this->error_code = ERR_NONE;
    this->error_state = 0;
    this->token_size = 0;
    this->read_timeout_ms = 0;  // No timeout by default
    this->write_timeout_ms = 0;  // No timeout by default
    this->connect_timeout_ms = 0;  // No timeout by default

    this->sockfd = socket(family_type, 
                         CSocket::DefaultSocketType, 0);
    
    if (!IsValidSocket(this->sockfd)) {
        this->error_code = ERR_NOSOCKET;
        this->error_state = SOCK_CREATE;
        return;
    }

#if defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS)
    this->flags = fcntl(this->sockfd, F_GETFL, 0);
    if (this->flags < 0) {
        this->error_code = ERR_NOSOCKET;
        this->error_state = SOCK_CREATE;
    }
#endif

    this->SafeClearBuffer(inbuffer, CSocket::MaxBufferSize);
    this->SafeClearBuffer(outbuffer, CSocket::MaxBufferSize);
}

/**
 * CSocket constructor with existing socket descriptor
 * \param existing_fd Existing socket descriptor (e.g., from Accept())
 * \param is_existing_socket Must be true to indicate this is an existing fd, not a family_type
 * 
 * Used for initializing CSocket-derived classes with accepted connections.
 * Does not create a new socket - uses the provided descriptor.
 */
CSocket::CSocket(socket_t existing_fd, bool is_existing_socket) {
    (void)is_existing_socket;  // Parameter just disambiguates from CSocket(int family_type)
    
    this->net_family = CSocket::DefaultFamilyType;
    this->connected = true;  // Assumed valid since it came from Accept()
    this->blocking = true;
    this->n = 0;
    this->port = 0;
    this->error_code = ERR_NONE;
    this->error_state = 0;
    this->token_size = 0;
    this->read_timeout_ms = 0;  // No timeout by default
    this->write_timeout_ms = 0;  // No timeout by default
    this->connect_timeout_ms = 0;  // No timeout by default
    
    // Use the existing socket descriptor
    this->sockfd = existing_fd;
    
#if defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS)
    this->flags = fcntl(this->sockfd, F_GETFL, 0);
    if (this->flags < 0) {
        this->error_code = ERR_NOSOCKET;
        this->error_state = SOCK_CREATE;
    }
#endif

    this->SafeClearBuffer(inbuffer, CSocket::MaxBufferSize);
    this->SafeClearBuffer(outbuffer, CSocket::MaxBufferSize);
}

/**
 * CSocket destructor - cleans up socket resources
 */
CSocket::~CSocket() {
#if defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS)
    if (this->connected) {
        close(this->sockfd);
    }
#endif

#ifdef PLATFORM_WINDOWS
    if (this->sockfd != INVALID_SOCKET) {
        closesocket(this->sockfd);
    }
    // WSACleanup() removed - global resource, not per-socket
#endif

    this->sockfd = INVALID_SOCKET_T;
}

/**
 * Close the socket connection
 * \return true if successful
 */
bool CSocket::Close() {
    if (!IsValidSocket(this->sockfd)) {
        return false;
    }

#ifdef PLATFORM_WINDOWS
    if (this->sockfd != INVALID_SOCKET) {
        closesocket(this->sockfd);
        this->sockfd = INVALID_SOCKET;
    }
    // WSACleanup() removed - global resource, not per-socket
#else
    if (this->connected) {
        close(this->sockfd);
        this->sockfd = -1;
    }
#endif

    this->connected = false;
    return true;
}

/**
 * Write char* data to socket
 * \param data Pointer to null-terminated string
 * \return Number of bytes written
 */
int CSocket::Write(char* data) {
    if (!data || !IsValidSocket(this->sockfd)) {
        return NET_SOCKET_ERROR;
    }
    
    std::size_t len = std::strlen(data);
    if (len == 0) {
        return 0;
    }
    
    int bytesSent = static_cast<int>(send(this->sockfd, data, static_cast<int>(len), 0));
    return bytesSent;
}

/**
 * Write const char* data to socket
 * \param data Pointer to null-terminated const string
 * \return Number of bytes written
 */
int CSocket::Write(const char* data) {
    return this->Write(const_cast<char*>(data));
}

/**
 * Write char* data to socket with explicit size
 * \param data Pointer to data buffer
 * \param size Number of bytes to write
 * \return Number of bytes written
 */
int CSocket::Write(char* data, int size) {
    if (!data || !IsValidSocket(this->sockfd) || size <= 0) {
        return NET_SOCKET_ERROR;
    }
    
    // If write timeout is set, wait with timeout
    if (this->write_timeout_ms > 0) {
        int wait_result = CSocket::WaitForWritable(this->sockfd, this->write_timeout_ms);
        if (wait_result == 0) {
            // Timeout - socket not ready to write
            return 0;
        } else if (wait_result < 0) {
            // Error
            this->error_code = GET_NET_SOCKET_ERROR();
            return NET_SOCKET_ERROR;
        }
    }
    
    int bytesSent = static_cast<int>(send(this->sockfd, data, size, CSocket::NULLFlag));
    return bytesSent;
}

/**
 * Write const char* data to socket with explicit size
 * \param data Pointer to const data buffer
 * \param size Number of bytes to write
 * \return Number of bytes written
 */
int CSocket::Write(const char* data, int size) {
    return this->Write(const_cast<char*>(data), size);
}

/**
 * Write std::string data to socket
 * \param data std::string to write
 * \return Number of bytes written
 */
int CSocket::Write(const std::string& data) {
    if (!IsValidSocket(this->sockfd) || data.empty()) {
        return NET_SOCKET_ERROR;
    }
    
    int bytesSent = static_cast<int>(send(this->sockfd, 
                        data.c_str(), 
                        static_cast<int>(data.size()), 
                        0));
    return bytesSent;
}

/**
 * Read data from socket into buffer
 * \param buffer Character array to receive data
 * \param size Maximum number of bytes to read
 * \return Number of bytes read (0 on timeout)
 */
int CSocket::Read(char* buffer, int size) {
    if (!buffer || !IsValidSocket(this->sockfd) || size <= 0) {
        return NET_SOCKET_ERROR;
    }

    // Validate and clamp size to prevent buffer overflow
    // Protect against INT_MAX and unreasonably large read sizes
    int safe_size = (size > CSocket::MaxBufferSize) 
                  ? CSocket::MaxBufferSize 
                  : size;

    // If read timeout is set, wait with timeout
    if (this->read_timeout_ms > 0) {
        int wait_result = CSocket::WaitForReadable(this->sockfd, this->read_timeout_ms);
        if (wait_result == 0) {
            // Timeout - no data available
            this->n = 0;
            return 0;
        } else if (wait_result < 0) {
            // Error in select()
            this->error_code = GET_NET_SOCKET_ERROR();
            this->n = NET_SOCKET_ERROR;
            return NET_SOCKET_ERROR;
        }
    }

    SafeClearBuffer(buffer, safe_size);
    int bytesRead = static_cast<int>(recv(this->sockfd, buffer, safe_size, 0));
    this->n = bytesRead;
    
    if (bytesRead == 0) {
        this->connected = false;
    } else if (bytesRead < 0) {
        this->error_code = GET_NET_SOCKET_ERROR();
    }
    
    return bytesRead;
}

/**
 * Read data from socket until size bytes received
 * \param buffer Character array to receive data
 * \param size Total number of bytes to read
 * \return Total number of bytes read
 */
int CSocket::ReadUntil(char* buffer, int size) {
    if (!buffer || !IsValidSocket(this->sockfd) || size <= 0) {
        return NET_SOCKET_ERROR;
    }

    // Validate and clamp size to prevent buffer overflow
    // Protect against INT_MAX and unreasonably large read sizes
    int safe_size = (size > CSocket::MaxBufferSize) 
                  ? CSocket::MaxBufferSize 
                  : size;

    SafeClearBuffer(buffer, safe_size);
    
    std::string accumulated;
    accumulated.reserve(safe_size);
    
    int totalBytes = 0;
    char tempBuffer[256];
    
    while (totalBytes < safe_size) {
        SafeClearBuffer(tempBuffer, sizeof(tempBuffer));
        int remaining = safe_size - totalBytes;
        int toRead = (remaining < static_cast<int>(sizeof(tempBuffer))) 
                    ? remaining 
                    : static_cast<int>(sizeof(tempBuffer));
        
        int bytesRead = static_cast<int>(recv(this->sockfd, tempBuffer, toRead, 0));
        this->n = bytesRead;
        
        if (bytesRead == 0) {
            this->connected = false;
            break;
        }
        
        if (bytesRead < 0) {
            this->error_code = GET_NET_SOCKET_ERROR();
            break;
        }
        
        accumulated.append(tempBuffer, bytesRead);
        totalBytes += bytesRead;
    }
    
    // Safe copy to output buffer
    if (!accumulated.empty()) {
        std::size_t copySize = (accumulated.size() < static_cast<std::size_t>(safe_size - 1))
                              ? accumulated.size()
                              : static_cast<std::size_t>(safe_size - 1);
        std::memcpy(buffer, accumulated.data(), copySize);
        buffer[copySize] = '\0';
    }
    
    return totalBytes;
}

/**
 * Read data from socket into internal buffer
 * \return Number of bytes read
 */
int CSocket::Read() {
    return this->Read(this->inbuffer, CSocket::MaxBufferSize - 1);
}

/**
 * Read data from socket as std::string
 * \param size Number of bytes to read
 * \return std::string containing read data
 */
std::string CSocket::Read(int size) {
    std::string retVal;
    
    if (!IsValidSocket(this->sockfd) || size <= 0) {
        return retVal;
    }

    // Validate and clamp size to prevent buffer overflow
    // Protect against INT_MAX and values exceeding MaxBufferSize
    int safe_size = (size > CSocket::MaxBufferSize - 1) 
                  ? CSocket::MaxBufferSize - 1 
                  : size;

    this->SafeClearBuffer(inbuffer, CSocket::MaxBufferSize);
    int bytesRead = static_cast<int>(recv(this->sockfd, this->inbuffer, safe_size, 0));
    
    this->n = bytesRead;
    
    if (bytesRead > 0) {
        this->inbuffer[bytesRead] = '\0';
        retVal = this->inbuffer;
    } else if (bytesRead == 0) {
        this->connected = false;
    } else {
        this->error_code = GET_NET_SOCKET_ERROR();
    }
    
    return retVal;
}

/**
 * Clear input and output buffers
 * \internal
 */
void CSocket::ClearBuffers() {
    this->SafeClearBuffer(inbuffer, CSocket::MaxBufferSize);
    this->SafeClearBuffer(outbuffer, CSocket::MaxBufferSize);
}

/**
 * Clear a specific buffer
 * \param buffer Buffer to clear
 * \param size Size of buffer
 * \internal
 */
void CSocket::ClearBuffer(char* buffer, int size) {
    SafeClearBuffer(buffer, size);
}

/**
 * Read a single line from socket (until CRLF)
 * \param buffer Character array to receive line
 * \param size Maximum bytes to read
 * \return Number of bytes read
 */
int CSocket::ReadLine(char* buffer, int size) {
    if (!buffer || !IsValidSocket(this->sockfd) || size <= 0) {
        return NET_SOCKET_ERROR;
    }

    // Validate and clamp size to prevent buffer overflow
    // Protect against INT_MAX and unreasonably large read sizes
    int safe_size = (size > CSocket::MaxBufferSize) 
                  ? CSocket::MaxBufferSize 
                  : size;

    SafeClearBuffer(buffer, safe_size);
    
    this->n = 0;
    bool bCarriage = false;
    bool bLinefeed = false;
    char tempBuff[1];

    for (int i = 0; i < safe_size - 1; ++i) {
        int bytesRead = static_cast<int>(recv(this->sockfd, tempBuff, 1, 0));
        
        if (bytesRead == 0) {
            this->connected = false;
            break;
        }
        
        if (bytesRead < 0) {
            this->error_code = GET_NET_SOCKET_ERROR();
            break;
        }
        
        this->n += bytesRead;
        buffer[i] = tempBuff[0];

        if (tempBuff[0] == '\r') {
            bCarriage = true;
        } else if (tempBuff[0] == '\n') {
            bLinefeed = true;
        }

        // Line terminator found (CRLF)
        if (bCarriage && bLinefeed) {
            buffer[std::min(i + 1, safe_size - 1)] = '\0';
            return this->n;
        }
    }
    
    // Ensure null termination
    buffer[safe_size - 1] = '\0';
    return this->n;
}

/**
 * Set socket blocking/non-blocking mode
 * \param flag true for blocking, false for non-blocking
 * \return 0 on success, -1 on error (check GetError() for details)
 * 
 * Changes the socket between blocking and non-blocking mode.
 * In blocking mode, operations wait until complete.
 * In non-blocking mode, operations return immediately if not ready.
 * 
 * Error code set on failure:
 * - ERR_NOSOCKET: Socket is invalid
 * - Platform errno/WSAError: System call failed
 */
int CSocket::SetBlocking(bool flag) {
    if (!IsValidSocket(this->sockfd)) {
        this->error_code = ERR_NOSOCKET;
        this->error_state = SOCK_CREATE;
        return -1;
    }

    int result = SetSocketNonblocking(!flag);
    if (result != 0) {
        this->error_code = GET_NET_SOCKET_ERROR();
        this->error_state = SOCK_CREATE;
        return -1;
    }
    
    this->blocking = flag;
    this->error_code = ERR_NONE;  // Clear previous errors on success
    return 0;  // Success
}


/**
 * Set read timeout
 * \param timeout_ms Timeout in milliseconds (0 = no timeout, blocking behavior)
 * \return 0 on success, -1 on error
 * 
 * When a read timeout is set, Read() operations will wait for the specified time
 * before returning 0 if no data is available. This allows for non-blocking reads
 * with timeout semantics in blocking sockets.
 * 
 * Error code set on failure:
 * - ERR_NOSOCKET: Socket is invalid
 */
int CSocket::SetReadTimeout(int timeout_ms) {
    if (timeout_ms < 0) {
        this->error_code = ERR_NOSOCKET;  // Invalid parameter
        return -1;
    }
    this->read_timeout_ms = timeout_ms;
    this->error_code = ERR_NONE;  // Clear previous errors on success
    return 0;
}

/**
 * Set write timeout
 * \param timeout_ms Timeout in milliseconds (0 = no timeout, blocking behavior)
 * \return 0 on success, -1 on error
 * 
 * When a write timeout is set, Write() operations will apply timeout semantics
 * if supported by the platform. Currently, this is primarily used for tracking
 * intent and may be used for future async write operations.
 * 
 * Error code set on failure:
 * - ERR_NOSOCKET: Socket is invalid
 */
int CSocket::SetWriteTimeout(int timeout_ms) {
    if (timeout_ms < 0) {
        this->error_code = ERR_NOSOCKET;  // Invalid parameter
        return -1;
    }
    this->write_timeout_ms = timeout_ms;
    this->error_code = ERR_NONE;  // Clear previous errors on success
    return 0;
}

/**
 * Set connect timeout
 * \param timeout_ms Timeout in milliseconds (0 = blocking, wait indefinitely)
 * \return 0 on success, -1 on error
 * 
 * When a connect timeout is set, Connect() operations will fail if they cannot
 * complete within the specified time. This is useful for network operations where
 * the server may be slow or unreachable.
 * 
 * Error code set on failure:
 * - ERR_NOSOCKET: Socket is invalid
 */
int CSocket::SetConnectTimeout(int timeout_ms) {
    if (timeout_ms < 0) {
        this->error_code = ERR_NOSOCKET;  // Invalid parameter
        return -1;
    }
    this->connect_timeout_ms = timeout_ms;
    this->error_code = ERR_NONE;  // Clear previous errors on success
    return 0;
}

/**
 * Set TCP_NODELAY (disable Nagle's algorithm)
 * \param enabled true to enable TCP_NODELAY (disable Nagle), false to disable (enable Nagle)
 * \return 0 on success, -1 on error (check GetError() for details)
 * 
 * TCP_NODELAY disables Nagle's algorithm, causing TCP to send packets immediately
 * without waiting to combine small packets. This reduces latency but may increase
 * bandwidth usage. Useful for interactive protocols like telnet, SSH, and games.
 * 
 * Error code set on failure:
 * - ERR_NOSOCKET: Socket is invalid
 * - Platform errno/WSAError: setsockopt() failed
 */
int CSocket::SetTCPNodelay(bool enabled) {
    if (!IsValidSocket(this->sockfd)) {
        this->error_code = ERR_NOSOCKET;
        this->error_state = SOCK_CREATE;
        return -1;
    }

    int flag = enabled ? 1 : 0;
    if (setsockopt(this->sockfd, IPPROTO_TCP, TCP_NODELAY, 
                   (const char*)&flag, sizeof(flag)) < 0) {
        this->error_code = GET_NET_SOCKET_ERROR();
        this->error_state = SOCK_CREATE;
        return -1;
    }
    
    this->error_code = ERR_NONE;  // Clear previous errors on success
    return 0;
}

/**
 * Set SO_REUSEADDR (allow rapid socket rebinding)
 * \param enabled true to enable SO_REUSEADDR, false to disable
 * \return 0 on success, -1 on error
 * 
 * SO_REUSEADDR allows a socket to bind to a port in TIME_WAIT state,
 * enabling rapid reconnection without waiting 30-120 seconds.
 * Essential for servers and client connection pools.
 */
int CSocket::SetSOReusAddr(bool enabled) {
    if (!IsValidSocket(this->sockfd)) {
        this->error_code = ERR_NOSOCKET;
        this->error_state = SOCK_CREATE;
        return -1;
    }

    int flag = enabled ? 1 : 0;
    if (setsockopt(this->sockfd, SOL_SOCKET, SO_REUSEADDR, 
                   (const char*)&flag, sizeof(flag)) < 0) {
        this->error_code = GET_NET_SOCKET_ERROR();
        this->error_state = SOCK_CREATE;
        return -1;
    }
    
    this->error_code = ERR_NONE;
    return 0;
}

/**
 * Set SO_LINGER (control socket closing behavior)
 * \param linger_time_sec Linger time in seconds (0 = disable, >0 = enable with timeout)
 * \return 0 on success, -1 on error
 * 
 * SO_LINGER controls whether close() waits for pending data to be sent.
 * - linger_time_sec = 0: Disable linger, close immediately (avoids TIME_WAIT)
 * - linger_time_sec > 0: Wait up to N seconds for pending data before closing
 * 
 * Set to 0 on server-side accepted connections to avoid TIME_WAIT delays
 * when restarting services.
 */
int CSocket::SetSOLinger(u_short linger_time_sec) {
    if (!IsValidSocket(this->sockfd)) {
        this->error_code = ERR_NOSOCKET;
        this->error_state = SOCK_CREATE;
        return -1;
    }

    struct linger linger_opt;
    linger_opt.l_onoff = (linger_time_sec > 0) ? 1 : 0;
    linger_opt.l_linger = linger_time_sec;
    
    if (setsockopt(this->sockfd, SOL_SOCKET, SO_LINGER, 
                   (const char*)&linger_opt, sizeof(linger_opt)) < 0) {
        this->error_code = GET_NET_SOCKET_ERROR();
        this->error_state = SOCK_CREATE;
        return -1;
    }
    
    this->error_code = ERR_NONE;
    return 0;
}
} // namespace net

#endif // _CSOCKET_CPP