/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/


#include <net.h>
#include <cstring>
#include <algorithm>

namespace net {


// Static helpers for socket option configuration
static inline void SetSocketReusAddr(socket_t sock) {
    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));
}

static inline void SetSocketTCPNodelay(socket_t sock) {
    int nodelay = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&nodelay, sizeof(nodelay));
}

static inline void SetSocketLinger(socket_t sock, u_short linger_sec) {
    struct linger linger_opt = {0, 0};
    linger_opt.l_onoff = (linger_sec > 0) ? 1 : 0;
    linger_opt.l_linger = linger_sec;
    setsockopt(sock, SOL_SOCKET, SO_LINGER, (const char*)&linger_opt, sizeof(linger_opt));
}


// Static WSAStartup initializer for Windows
#ifdef NET_PLATFORM_WINDOWS
class WindowsSocketInit {
public:
    WindowsSocketInit() {
        WSADATA wsadata;
        WSAStartup(MAKEWORD(2, 2), &wsadata);
    }
};
static WindowsSocketInit g_wsa_init;
#endif

// Helper: Convert mode parameter for ioctlsocket (handles MinGW/MSVC type differences)
// Both MSVC and MinGW expect u_long* for the third parameter
static inline int set_socket_nonblocking(socket_t sockfd, bool nonblocking) {
#ifdef NET_PLATFORM_WINDOWS
    u_long mode = nonblocking ? 1 : 0;
    return ioctlsocket(sockfd, FIONBIO, &mode);
#else
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags < 0) return -1;
    
    if (nonblocking) {
        return fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
    } else {
        return fcntl(sockfd, F_SETFL, flags);
    }
#endif
}

// Helper: Copy string safely to fixed-size buffer
static inline bool safe_string_copy(char* dest, const char* src, std::size_t dest_size) {
    if (!dest || !src || dest_size == 0) {
        return false;
    }
    std::size_t src_len = std::strlen(src);
    if (src_len >= dest_size) {
        // Source is too large for destination
        std::strncpy(dest, src, dest_size - 1);
        dest[dest_size - 1] = '\0';
        return false;
    }
    std::strcpy(dest, src);
    return true;
}

// Helper: Safe buffer clearing
static inline void safe_clear_buffer(char* buffer, std::size_t size) {
    if (buffer && size > 0) {
        std::memset(buffer, 0, size);
    }
}

/**
 * blocking_mode_guard implementation
 * Manages socket blocking mode with RAII semantics
 */
blocking_mode_guard::blocking_mode_guard(socket* socket) 
    : socket_(socket), original_blocking_(false), valid_(false) {
    
    if (!socket_) {
        return;
    }
    
    original_blocking_ = socket_->blocking;
    
    // Attempt to set non-blocking mode
    if (socket_->set_blocking(false) == 0) {
        // Success - mark guard as valid so destructor will restore
        valid_ = true;
    }
    // If set_blocking failed, valid_ remains false and destructor won't restore
}

blocking_mode_guard::~blocking_mode_guard() {
    // Only restore if we successfully entered non-blocking mode
    if (valid_ && socket_) {
        // Ignore restoration errors - we tried our best
        socket_->set_blocking(original_blocking_);
    }
}

// Helper: Wait for socket to be readable with timeout
int socket::wait_for_readable(socket_t sockfd, int timeout_ms) {
    if (timeout_ms <= 0) {
        return 1;  // No timeout, proceed immediately
    }

#ifdef NET_PLATFORM_WINDOWS
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
int socket::wait_for_writable(socket_t sockfd, int timeout_ms) {
    if (timeout_ms <= 0) {
        return 1;  // No timeout, proceed immediately
    }

#ifdef NET_PLATFORM_WINDOWS
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
 * socket operator<< for char* data
 * \param data Pointer to null-terminated string
 * \return Number of bytes written to socket
 */
int socket::operator<<(char* data) {
    if (!data) {
        return NET_SOCKET_ERROR;
    }
    return this->write(data);
}

/**
 * socket operator<< for std::string data
 * \param data std::string object to write
 * \return Number of bytes written to socket
 */
int socket::operator<<(const std::string& data) {
    return this->write(data);
}

/**
 * socket operator>> for reading data
 * \param (unused parameter for operator syntax)
 * \return std::string containing read data
 */
std::string socket::operator>>(std::string) {
    return this->read(socket::max_buffer_size);
}

/**
 * get_state retrieves the current state of the socket
 * \return The current state of the socket
 */
int socket::get_state() const {
    return this->error_state;
}

/**
 * get_error retrieves the current error code in the socket object
 * \return The error code currently stored
 */
int socket::get_error() const {
    return this->error_code;
}

/**
 * set_error sets the current error code in the socket object
 * \param error The error code to set
 */
void socket::set_error(int error) {
    this->error_code = error;
}

/**
 * get_remote_ip returns the remote address as a human-readable string
 * \return IP address string (IPv4 or IPv6), or empty string if not set
 */
std::string socket::get_remote_ip() const {
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
 * get_remote_port returns the remote port number
 * \return Port number (0-65535), or 0 if not set
 */
int socket::get_remote_port() const {
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
 * get_remote_address returns the remote address as an address object
 * \return address wrapping the remote peer's address and port
 */
address socket::get_remote_address() const {
    return address(remote_addr);
}

/**
 * socket constructor - initializes socket with default settings
 */
socket::socket() {
    this->net_family = socket::default_family;
    this->connected = false;
    this->blocking = true;
    this->n = 0;
    this->port = 0;
    this->error_code = err_none;
    this->error_state = 0;
    this->token_size = 0;
    this->read_timeout_ms = 0;  // No timeout by default
    this->write_timeout_ms = 0;  // No timeout by default
    this->connect_timeout_ms = 0;  // No timeout by default

    // Create socket
    this->sockfd = ::socket(socket::default_family, 
                         socket::default_type, 0);
    
    if (!this->is_valid_socket()) {
        this->error_code = err_no_socket;
        this->error_state = state_create;
        return;
    }

#if defined(NET_PLATFORM_LINUX) || defined(NET_PLATFORM_MACOS)
    this->flags = fcntl(this->sockfd, F_GETFL, 0);
    if (this->flags < 0) {
        this->error_code = err_no_socket;
        this->error_state = state_create;
    }
#endif

    // Clear buffers
    safe_clear_buffer(this->inbuffer, socket::max_buffer_size);
    safe_clear_buffer(this->outbuffer, socket::max_buffer_size);
}

/**
 * socket constructor with family type
 * \param family_type The address family type (AF_INET, AF_INET6, etc.)
 */
socket::socket(int family_type) {
    this->net_family = family_type;
    this->connected = false;
    this->blocking = true;
    this->n = 0;
    this->port = 0;
    this->error_code = err_none;
    this->error_state = 0;
    this->token_size = 0;
    this->read_timeout_ms = 0;  // No timeout by default
    this->write_timeout_ms = 0;  // No timeout by default
    this->connect_timeout_ms = 0;  // No timeout by default

    this->sockfd = ::socket(family_type, 
                         socket::default_type, 0);
    
    if (!this->is_valid_socket()) {
        this->error_code = err_no_socket;
        this->error_state = state_create;
        return;
    }

#if defined(NET_PLATFORM_LINUX) || defined(NET_PLATFORM_MACOS)
    this->flags = fcntl(this->sockfd, F_GETFL, 0);
    if (this->flags < 0) {
        this->error_code = err_no_socket;
        this->error_state = state_create;
    }
#endif

    safe_clear_buffer(this->inbuffer, socket::max_buffer_size);
    safe_clear_buffer(this->outbuffer, socket::max_buffer_size);
}

/**
 * Constructor with explicit family and type
 * \param family Address family (AF_INET, AF_INET6, etc)
 * \param type Socket type (SOCK_STREAM, SOCK_DGRAM, etc)
 */
socket::socket(int family, int type) {
    this->net_family = family;
    this->connected = false;
    this->blocking = true;
    this->n = 0;
    this->port = 0;
    this->error_code = err_none;
    this->error_state = 0;
    this->token_size = 0;
    this->read_timeout_ms = 0;  // No timeout by default
    this->write_timeout_ms = 0;  // No timeout by default
    this->connect_timeout_ms = 0;  // No timeout by default

    this->sockfd = ::socket(family, type, 0);
    
    if (!this->is_valid_socket()) {
        this->error_code = err_no_socket;
        this->error_state = state_create;
        return;
    }

#if defined(NET_PLATFORM_LINUX) || defined(NET_PLATFORM_MACOS)
    this->flags = fcntl(this->sockfd, F_GETFL, 0);
    if (this->flags < 0) {
        this->error_code = err_no_socket;
        this->error_state = state_create;
    }
#endif

    safe_clear_buffer(this->inbuffer, socket::max_buffer_size);
    safe_clear_buffer(this->outbuffer, socket::max_buffer_size);
}

/**
 * socket constructor with existing socket descriptor
 * \param existing_fd Existing socket descriptor (e.g., from accept())
 * \param is_existing_socket Must be true to indicate this is an existing fd, not a family_type
 * 
 * Used for initializing socket-derived classes with accepted connections.
 * Does not create a new socket - uses the provided descriptor.
 */
socket::socket(socket_t existing_fd, bool is_existing_socket) {
    (void)is_existing_socket;  // Parameter just disambiguates from socket(int family_type)
    
    this->net_family = socket::default_family;
    this->connected = true;  // Assumed valid since it came from accept()
    this->blocking = true;
    this->n = 0;
    this->port = 0;
    this->error_code = err_none;
    this->error_state = 0;
    this->token_size = 0;
    this->read_timeout_ms = 0;  // No timeout by default
    this->write_timeout_ms = 0;  // No timeout by default
    this->connect_timeout_ms = 0;  // No timeout by default
    
    // Use the existing socket descriptor
    this->sockfd = existing_fd;
    
#if defined(NET_PLATFORM_LINUX) || defined(NET_PLATFORM_MACOS)
    this->flags = fcntl(this->sockfd, F_GETFL, 0);
    if (this->flags < 0) {
        this->error_code = err_no_socket;
        this->error_state = state_create;
    }
#endif

    safe_clear_buffer(this->inbuffer, socket::max_buffer_size);
    safe_clear_buffer(this->outbuffer, socket::max_buffer_size);
}

/**
 * socket destructor - cleans up socket resources
 */
socket::~socket() {
#if defined(NET_PLATFORM_LINUX) || defined(NET_PLATFORM_MACOS)
    if (this->connected) {
        close(this->sockfd);
    }
#endif

#ifdef NET_PLATFORM_WINDOWS
    if (this->sockfd != INVALID_SOCKET) {
        closesocket(this->sockfd);
    }
    // WSACleanup() removed - global resource, not per-socket
#endif

    this->sockfd = invalid_socket;
}

/**
 * close the socket connection
 * \return true if successful
 */
bool socket::close() {
    if (!this->is_valid_socket()) {
        return false;
    }

#ifdef NET_PLATFORM_WINDOWS
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
 * Check if this socket's descriptor is valid
 * \return true if the socket descriptor is valid, false otherwise
 */
bool socket::is_valid_socket() const {
#ifdef NET_PLATFORM_WINDOWS
    return this->sockfd != INVALID_SOCKET;
#else
    return this->sockfd >= 0;
#endif
}

/**
 * write char* data to socket
 * \param data Pointer to null-terminated string
 * \return Number of bytes written
 */
int socket::write(char* data) {
    if (!data || !this->is_valid_socket()) {
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
 * write const char* data to socket
 * \param data Pointer to null-terminated const string
 * \return Number of bytes written
 */
int socket::write(const char* data) {
    return this->write(const_cast<char*>(data));
}

/**
 * write char* data to socket with explicit size
 * \param data Pointer to data buffer
 * \param size Number of bytes to write
 * \return Number of bytes written
 */
int socket::write(char* data, int size) {
    if (!data || !this->is_valid_socket() || size <= 0) {
        return NET_SOCKET_ERROR;
    }
    
    // If write timeout is set, wait with timeout
    if (this->write_timeout_ms > 0) {
        int wait_result = socket::wait_for_writable(this->sockfd, this->write_timeout_ms);
        if (wait_result == 0) {
            // Timeout - socket not ready to write
            return 0;
        } else if (wait_result < 0) {
            // Error
            this->error_code = GET_NET_SOCKET_ERROR();
            return NET_SOCKET_ERROR;
        }
    }
    
    int bytesSent = static_cast<int>(send(this->sockfd, data, size, 0));
    return bytesSent;
}

/**
 * write const char* data to socket with explicit size
 * \param data Pointer to const data buffer
 * \param size Number of bytes to write
 * \return Number of bytes written
 */
int socket::write(const char* data, int size) {
    return this->write(const_cast<char*>(data), size);
}

/**
 * write std::string data to socket
 * \param data std::string to write
 * \return Number of bytes written
 */
int socket::write(const std::string& data) {
    if (!this->is_valid_socket() || data.empty()) {
        return NET_SOCKET_ERROR;
    }
    
    int bytesSent = static_cast<int>(send(this->sockfd, 
                        data.c_str(), 
                        static_cast<int>(data.size()), 
                        0));
    return bytesSent;
}

/**
 * read data from socket into buffer
 * \param buffer Character array to receive data
 * \param size Maximum number of bytes to read
 * \return Number of bytes read (0 on timeout)
 */
int socket::read(char* buffer, int size) {
    if (!buffer || !this->is_valid_socket() || size <= 0) {
        return NET_SOCKET_ERROR;
    }

    // Validate and clamp size to prevent buffer overflow
    // Protect against INT_MAX and unreasonably large read sizes
    int safe_size = (size > socket::max_buffer_size) 
                  ? socket::max_buffer_size 
                  : size;

    // If read timeout is set, wait with timeout
    if (this->read_timeout_ms > 0) {
        int wait_result = socket::wait_for_readable(this->sockfd, this->read_timeout_ms);
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

    safe_clear_buffer(buffer, safe_size);
    int bytesread = static_cast<int>(recv(this->sockfd, buffer, safe_size, 0));
    this->n = bytesread;
    
    if (bytesread == 0) {
        this->connected = false;
    } else if (bytesread < 0) {
        this->error_code = GET_NET_SOCKET_ERROR();
    }
    
    return bytesread;
}

/**
 * read data from socket until size bytes received
 * \param buffer Character array to receive data
 * \param size Total number of bytes to read
 * \return Total number of bytes read
 */
int socket::read_until(char* buffer, int size) {
    if (!buffer || !this->is_valid_socket() || size <= 0) {
        return NET_SOCKET_ERROR;
    }

    // Validate and clamp size to prevent buffer overflow
    // Protect against INT_MAX and unreasonably large read sizes
    int safe_size = (size > socket::max_buffer_size) 
                  ? socket::max_buffer_size 
                  : size;

    safe_clear_buffer(buffer, safe_size);
    
    std::string accumulated;
    accumulated.reserve(safe_size);
    
    int totalBytes = 0;
    char tempBuffer[256];
    
    while (totalBytes < safe_size) {
        safe_clear_buffer(tempBuffer, sizeof(tempBuffer));
        int remaining = safe_size - totalBytes;
        int toread = (remaining < static_cast<int>(sizeof(tempBuffer))) 
                    ? remaining 
                    : static_cast<int>(sizeof(tempBuffer));
        
        int bytesread = static_cast<int>(recv(this->sockfd, tempBuffer, toread, 0));
        this->n = bytesread;
        
        if (bytesread == 0) {
            this->connected = false;
            break;
        }
        
        if (bytesread < 0) {
            this->error_code = GET_NET_SOCKET_ERROR();
            break;
        }
        
        accumulated.append(tempBuffer, bytesread);
        totalBytes += bytesread;
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
 * read data from socket into internal buffer
 * \return Number of bytes read
 */
int socket::read() {
    return this->read(this->inbuffer, socket::max_buffer_size - 1);
}

/**
 * read data from socket as std::string
 * \param size Number of bytes to read
 * \return std::string containing read data
 */
std::string socket::read(int size) {
    std::string retVal;
    
    if (!this->is_valid_socket() || size <= 0) {
        return retVal;
    }

    // Validate and clamp size to prevent buffer overflow
    // Protect against INT_MAX and values exceeding max_buffer_size
    int safe_size = (size > socket::max_buffer_size - 1) 
                  ? socket::max_buffer_size - 1 
                  : size;

    safe_clear_buffer(this->inbuffer, socket::max_buffer_size);
    int bytesread = static_cast<int>(recv(this->sockfd, this->inbuffer, safe_size, 0));
    
    this->n = bytesread;
    
    if (bytesread > 0) {
        this->inbuffer[bytesread] = '\0';
        retVal = this->inbuffer;
    } else if (bytesread == 0) {
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
void socket::clear_buffers() {
    safe_clear_buffer(this->inbuffer, socket::max_buffer_size);
    safe_clear_buffer(this->outbuffer, socket::max_buffer_size);
}

/**
 * Clear a specific buffer
 * \param buffer Buffer to clear
 * \param size Size of buffer
 * \internal
 */
void socket::clear_buffer(char* buffer, int size) {
    safe_clear_buffer(buffer, size);
}

/**
 * read a single line from socket (until CRLF)
 * \param buffer Character array to receive line
 * \param size Maximum bytes to read
 * \return Number of bytes read
 */
int socket::read_line(char* buffer, int size) {
    if (!buffer || !this->is_valid_socket() || size <= 0) {
        return NET_SOCKET_ERROR;
    }

    // Validate and clamp size to prevent buffer overflow
    // Protect against INT_MAX and unreasonably large read sizes
    int safe_size = (size > socket::max_buffer_size) 
                  ? socket::max_buffer_size 
                  : size;

    safe_clear_buffer(buffer, safe_size);
    
    this->n = 0;
    bool bCarriage = false;
    bool bLinefeed = false;
    char tempBuff[1];

    for (int i = 0; i < safe_size - 1; ++i) {
        int bytesread = static_cast<int>(recv(this->sockfd, tempBuff, 1, 0));
        
        if (bytesread == 0) {
            this->connected = false;
            break;
        }
        
        if (bytesread < 0) {
            this->error_code = GET_NET_SOCKET_ERROR();
            break;
        }
        
        this->n += bytesread;
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
 * \return 0 on success, -1 on error (check get_error() for details)
 * 
 * Changes the socket between blocking and non-blocking mode.
 * In blocking mode, operations wait until complete.
 * In non-blocking mode, operations return immediately if not ready.
 * 
 * Error code set on failure:
 * - err_no_socket: Socket is invalid
 * - Platform errno/WSAError: System call failed
 */
int socket::set_blocking(bool flag) {
    if (!this->is_valid_socket()) {
        this->error_code = err_no_socket;
        this->error_state = state_create;
        return -1;
    }

    int result = this->set_socket_nonblocking(!flag);
    if (result != 0) {
        this->error_code = GET_NET_SOCKET_ERROR();
        this->error_state = state_create;
        return -1;
    }
    
    this->blocking = flag;
    this->error_code = err_none;  // Clear previous errors on success
    return 0;  // Success
}


/**
 * Set read timeout
 * \param timeout_ms Timeout in milliseconds (0 = no timeout, blocking behavior)
 * \return 0 on success, -1 on error
 * 
 * When a read timeout is set, read() operations will wait for the specified time
 * before returning 0 if no data is available. This allows for non-blocking reads
 * with timeout semantics in blocking sockets.
 * 
 * Error code set on failure:
 * - err_no_socket: Socket is invalid
 */
int socket::set_read_timeout(int timeout_ms) {
    if (timeout_ms < 0) {
        this->error_code = err_no_socket;  // Invalid parameter
        return -1;
    }
    this->read_timeout_ms = timeout_ms;
    this->error_code = err_none;  // Clear previous errors on success
    return 0;
}

/**
 * Set write timeout
 * \param timeout_ms Timeout in milliseconds (0 = no timeout, blocking behavior)
 * \return 0 on success, -1 on error
 * 
 * When a write timeout is set, write() operations will apply timeout semantics
 * if supported by the platform. Currently, this is primarily used for tracking
 * intent and may be used for future async write operations.
 * 
 * Error code set on failure:
 * - err_no_socket: Socket is invalid
 */
int socket::set_write_timeout(int timeout_ms) {
    if (timeout_ms < 0) {
        this->error_code = err_no_socket;  // Invalid parameter
        return -1;
    }
    this->write_timeout_ms = timeout_ms;
    this->error_code = err_none;  // Clear previous errors on success
    return 0;
}

/**
 * Set connect timeout
 * \param timeout_ms Timeout in milliseconds (0 = blocking, wait indefinitely)
 * \return 0 on success, -1 on error
 * 
 * When a connect timeout is set, connect() operations will fail if they cannot
 * complete within the specified time. This is useful for network operations where
 * the server may be slow or unreachable.
 * 
 * Error code set on failure:
 * - err_no_socket: Socket is invalid
 */
int socket::set_connect_timeout(int timeout_ms) {
    if (timeout_ms < 0) {
        this->error_code = err_no_socket;  // Invalid parameter
        return -1;
    }
    this->connect_timeout_ms = timeout_ms;
    this->error_code = err_none;  // Clear previous errors on success
    return 0;
}

/**
 * Set TCP_NODELAY (disable Nagle's algorithm)
 * \param enabled true to enable TCP_NODELAY (disable Nagle), false to disable (enable Nagle)
 * \return 0 on success, -1 on error (check get_error() for details)
 * 
 * TCP_NODELAY disables Nagle's algorithm, causing TCP to send packets immediately
 * without waiting to combine small packets. This reduces latency but may increase
 * bandwidth usage. Useful for interactive protocols like telnet, SSH, and games.
 * 
 * Error code set on failure:
 * - err_no_socket: Socket is invalid
 * - Platform errno/WSAError: setsockopt() failed
 */
int socket::set_tcp_nodelay(bool enabled) {
    if (!this->is_valid_socket()) {
        this->error_code = err_no_socket;
        this->error_state = state_create;
        return -1;
    }

    int flag = enabled ? 1 : 0;
    if (setsockopt(this->sockfd, IPPROTO_TCP, TCP_NODELAY, 
                   (const char*)&flag, sizeof(flag)) < 0) {
        this->error_code = GET_NET_SOCKET_ERROR();
        this->error_state = state_create;
        return -1;
    }
    
    this->error_code = err_none;  // Clear previous errors on success
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
int socket::set_so_reuseaddr(bool enabled) {
    if (!this->is_valid_socket()) {
        this->error_code = err_no_socket;
        this->error_state = state_create;
        return -1;
    }

    int flag = enabled ? 1 : 0;
    if (setsockopt(this->sockfd, SOL_SOCKET, SO_REUSEADDR, 
                   (const char*)&flag, sizeof(flag)) < 0) {
        this->error_code = GET_NET_SOCKET_ERROR();
        this->error_state = state_create;
        return -1;
    }
    
    this->error_code = err_none;
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
int socket::set_so_linger(u_short linger_time_sec) {
    if (!this->is_valid_socket()) {
        this->error_code = err_no_socket;
        this->error_state = state_create;
        return -1;
    }

    struct linger linger_opt;
    linger_opt.l_onoff = (linger_time_sec > 0) ? 1 : 0;
    linger_opt.l_linger = linger_time_sec;
    
    if (setsockopt(this->sockfd, SOL_SOCKET, SO_LINGER, 
                   (const char*)&linger_opt, sizeof(linger_opt)) < 0) {
        this->error_code = GET_NET_SOCKET_ERROR();
        this->error_state = state_create;
        return -1;
    }
    
    this->error_code = err_none;
    return 0;
}

/**
 * Set IPV6_V6ONLY socket option
 * \param enabled true to disable dual-stack (IPv6 only), false for dual-stack
 * \return 0 on success, -1 on error
 */
int socket::set_ipv6_v6only(bool enabled) {
    if (!this->is_valid_socket()) {
        this->error_code = err_no_socket;
        this->error_state = state_create;
        return -1;
    }

#ifdef IPV6_V6ONLY
    int v6only = enabled ? 1 : 0;
    if (setsockopt(this->sockfd, IPPROTO_IPV6, IPV6_V6ONLY, 
                   (const char*)&v6only, sizeof(v6only)) < 0) {
        this->error_code = GET_NET_SOCKET_ERROR();
        this->error_state = state_create;
        return -1;
    }
    this->error_code = err_none;
    return 0;
#else
    // IPV6_V6ONLY not available on this platform
    return 0;
#endif
}



/**
 * Set socket to non-blocking mode
 * \param nonblocking true for non-blocking, false for blocking
 * \return 0 on success, -1 on error
 */
int socket::set_socket_nonblocking(bool nonblocking) {
    if (!this->is_valid_socket()) {
        this->error_code = err_no_socket;
        return -1;
    }

#ifdef NET_PLATFORM_WINDOWS
    u_long mode = nonblocking ? 1 : 0;
    int result = ioctlsocket(this->sockfd, FIONBIO, &mode);
#else
    int flags = fcntl(this->sockfd, F_GETFL, 0);
    if (flags < 0) return -1;
    
    int result = fcntl(this->sockfd, F_SETFL, nonblocking ? (flags | O_NONBLOCK) : flags);
#endif

    if (result != 0) {
        this->error_code = GET_NET_SOCKET_ERROR();
        return -1;
    }

    return 0;
}

/**
 * Safe buffer clearing utility
 * \param buffer Buffer to clear
 * \param size Size of buffer
 */
void socket::safe_clear_buffer(char* buffer, std::size_t size) {
    if (buffer && size > 0) {
        std::memset(buffer, 0, size);
    }
}

/**
 * Configure socket for connection attempt
 * Sets appropriate socket options before connecting
 * \internal
 */
void socket::configure_socket_for_connect() {
    if (!this->is_valid_socket()) {
        return;
    }

    // Could add more configuration here if needed
    // For now, this is a placeholder that can be extended
}

/**
 * Set socket SO_REUSEADDR option (public instance method)
 * Allows rapid rebinding of the socket
 */
void socket::set_socket_reuseaddr() {
    if (!this->is_valid_socket()) {
        return;
    }
    int reuse = 1;
    setsockopt(this->sockfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));
}

/**
 * Set socket TCP_NODELAY option (public instance method)
 * Disables Nagle's algorithm for lower latency
 */
void socket::set_socket_tcp_nodelay() {
    if (!this->is_valid_socket()) {
        return;
    }
    int nodelay = 1;
    setsockopt(this->sockfd, IPPROTO_TCP, TCP_NODELAY, (const char*)&nodelay, sizeof(nodelay));
}

/**
 * Set socket SO_LINGER option (public instance method)
 * Controls socket closing behavior
 * \param linger_sec Linger time in seconds (0 = disable)
 */
void socket::set_socket_linger(u_short linger_sec) {
    if (!this->is_valid_socket()) {
        return;
    }
    struct linger linger_opt = {0, 0};
    linger_opt.l_onoff = (linger_sec > 0) ? 1 : 0;
    linger_opt.l_linger = linger_sec;
    setsockopt(this->sockfd, SOL_SOCKET, SO_LINGER, (const char*)&linger_opt, sizeof(linger_opt));
}
} // namespace net