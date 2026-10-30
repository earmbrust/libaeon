/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/


#include <aeon.hpp>
#include <cstring>
#include <algorithm>

namespace aeon {

namespace internal {
struct socket_private_state {
    std::string hostname;
    socket* parent_server = nullptr;
};
} // namespace internal

void socket::set_parent_server(socket* server) {
    if (!state_) state_ = new internal::socket_private_state;
    state_->parent_server = server;
}
socket* socket::parent_server() const noexcept {
    return state_ ? state_->parent_server : nullptr;
}

void socket::set_remote_hostname(const std::string& hostname) {
    // Do not allocate per socket until a named destination is used.
    if (!state_) state_ = new internal::socket_private_state;
    state_->hostname = hostname;
}

std::string socket::remote_hostname() const {
    return state_ ? state_->hostname : std::string{};
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
        const int previous_error = socket_->get_error();
        if (socket_->set_blocking(original_blocking_) == 0 && previous_error != err_none)
            socket_->set_error(previous_error);
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
    this->connected = (existing_fd != invalid_socket);
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
    if (this->is_valid_socket()) {
        sockaddr_storage peer{};
        socklen_t length = static_cast<socklen_t>(sizeof(peer));
        if (::getpeername(this->sockfd,
                          reinterpret_cast<sockaddr*>(&peer), &length) == 0) {
            this->remote_addr = peer;
            this->net_family = peer.ss_family;
        }
    }

#if defined(NET_PLATFORM_LINUX) || defined(NET_PLATFORM_MACOS)
    if (this->is_valid_socket()) {
        this->flags = fcntl(this->sockfd, F_GETFL, 0);
        if (this->flags < 0) {
            this->error_code = err_no_socket;
            this->error_state = state_create;
        }
    }
#endif

    safe_clear_buffer(this->inbuffer, socket::max_buffer_size);
    safe_clear_buffer(this->outbuffer, socket::max_buffer_size);
}

/**
 * socket destructor - cleans up socket resources
 */
socket::~socket() {
    // Valid descriptors are owned regardless of the TCP connected flag:
    // unconnected TCP sockets and unbound UDP sockets need cleanup too.
    if (this->is_valid_socket()) {
        (void)this->close();
    }
    delete state_;
}

/**
 * close the socket connection
 * \return true if successful
 */
bool socket::close() {
    if (!this->is_valid_socket()) {
        return false;
    }

    // Relinquish ownership before calling the OS, so neither this object nor
    // its derived/base destructors can attempt a second close.
    socket_t owned = this->sockfd;
    this->sockfd = invalid_socket;
    this->connected = false;
    if (NET_CLOSE_SOCKET(owned) != 0) {
        this->error_code = internal::translate_native_error(GET_NET_SOCKET_ERROR());
        return false;
    }
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
        this->error_code = !this->is_valid_socket() ? err_no_socket : err_invalid_argument;
        return NET_SOCKET_ERROR;
    }
    
    std::size_t len = std::strlen(data);
    if (len == 0) {
        return 0;
    }
    
    int bytesSent = static_cast<int>(send(this->sockfd, data, static_cast<int>(len), 0));
    this->error_code = bytesSent < 0
        ? internal::translate_native_error(GET_NET_SOCKET_ERROR()) : err_none;
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
 * \return Bytes sent, 0 on readiness timeout, -1 on error
 *
 * A configured write timeout waits for writability before send(). A successful
 * wait does not guarantee that all requested bytes are sent.
 */
int socket::write(char* data, int size) {
    if (!data || !this->is_valid_socket() || size <= 0) {
        this->error_code = !this->is_valid_socket() ? err_no_socket : err_invalid_argument;
        return NET_SOCKET_ERROR;
    }
    
    // If write timeout is set, wait with timeout
    if (this->write_timeout_ms > 0) {
        int wait_result = socket::wait_for_writable(this->sockfd, this->write_timeout_ms);
        if (wait_result == 0) {
            // Timeout - socket not ready to write
            this->error_code = err_timeout;
            return 0;
        } else if (wait_result < 0) {
            // Error
            this->error_code = internal::translate_native_error(GET_NET_SOCKET_ERROR());
            return NET_SOCKET_ERROR;
        }
    }
    
    int bytesSent = static_cast<int>(send(this->sockfd, data, size, 0));
    this->error_code = bytesSent < 0
        ? internal::translate_native_error(GET_NET_SOCKET_ERROR()) : err_none;
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
 *
 * This overload sends directly without applying the configured write timeout.
 */
int socket::write(const std::string& data) {
    if (!this->is_valid_socket() || data.empty()) {
        this->error_code = !this->is_valid_socket() ? err_no_socket : err_invalid_argument;
        return NET_SOCKET_ERROR;
    }
    
    int bytesSent = static_cast<int>(send(this->sockfd, 
                        data.c_str(), 
                        static_cast<int>(data.size()), 
                        0));
    this->error_code = bytesSent < 0
        ? internal::translate_native_error(GET_NET_SOCKET_ERROR()) : err_none;
    return bytesSent;
}

/**
 * read data from socket into buffer
 * \param buffer Character array to receive data
 * \param size Maximum number of bytes to read
 * \return Bytes read, 0 on timeout or TCP EOF, -1 on error
 *
 * The supplied capacity is used without NUL termination. TCP EOF clears
 * connected; a read timeout leaves the connection state unchanged.
 */
int socket::read(char* buffer, int size) {
    if (!buffer || !this->is_valid_socket() || size <= 0) {
        this->error_code = !this->is_valid_socket() ? err_no_socket : err_invalid_argument;
        this->n = NET_SOCKET_ERROR;
        return NET_SOCKET_ERROR;
    }

    // The explicit size is the caller's capacity, not the internal legacy
    // 256-byte buffer size. Received bytes may contain embedded NULs; do not
    // clear, terminate, or otherwise modify the caller's remaining capacity.
    if (this->read_timeout_ms > 0) {
        const int ready = socket::wait_for_readable(this->sockfd, this->read_timeout_ms);
        if (ready == 0) {
            this->error_code = err_timeout;
            this->n = 0;
            return 0;
        }
        if (ready < 0) {
            this->error_code = internal::translate_native_error(GET_NET_SOCKET_ERROR());
            this->n = NET_SOCKET_ERROR;
            return NET_SOCKET_ERROR;
        }
    }

    const int received = static_cast<int>(::recv(this->sockfd, buffer, size, 0));
    this->n = received;
    if (received == 0) {
        this->connected = false;
        this->error_code = err_none;
    } else if (received < 0) {
        this->error_code = internal::translate_native_error(GET_NET_SOCKET_ERROR());
    } else {
        this->error_code = err_none;
    }
    return received;
}

/**
 * read data from socket until size bytes received
 * \param buffer Character array to receive data
 * \param size Total number of bytes to read
 * \return Total number of bytes read
 */
int socket::read_until(char* buffer, int size) {
    if (!buffer || !this->is_valid_socket() || size <= 0) {
        this->error_code = !this->is_valid_socket() ? err_no_socket : err_invalid_argument;
        this->n = NET_SOCKET_ERROR;
        return NET_SOCKET_ERROR;
    }

    // Consume exactly size bytes when available. Never reserve one byte for
    // a terminator: this method receives binary streams, not C strings.
    int total = 0;
    while (total < size) {
        const int received = this->read(buffer + total, size - total);
        if (received <= 0) break; // EOF, timeout, or OS error: preserve partial data.
        total += received;
    }
    return total;
}

/**
 * read data from socket into internal buffer
 * \return Number of bytes read
 */
int socket::read() {
    const int received = this->read(this->inbuffer, socket::max_buffer_size - 1);
    if (received >= 0) this->inbuffer[received] = '\0';
    return received;
}

/**
 * read data from socket as std::string
 * \param size Number of bytes to read
 * \return std::string containing read data
 */
std::string socket::read(int size) {
    if (!this->is_valid_socket() || size <= 0) return {};

    // Allocate for the requested maximum and resize to recv's byte count;
    // constructing from a NUL-terminated inbuffer would discard binary data.
    std::string result(static_cast<std::size_t>(size), '\0');
    const int received = this->read(result.data(), size);
    if (received <= 0) return {};
    result.resize(static_cast<std::size_t>(received));
    return result;
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
 *
 * Uses a fixed-size legacy buffer and does not apply read_timeout_ms.
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
            this->error_code = internal::translate_native_error(GET_NET_SOCKET_ERROR());
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
        this->error_code = internal::translate_native_error(GET_NET_SOCKET_ERROR());
        this->error_state = state_create;
        return -1;
    }
    
    this->blocking = flag;
    this->error_code = err_none;  // Clear previous errors on success
    return 0;  // Success
}


/**
 * Set read timeout
 * \param timeout_ms Timeout in milliseconds (0 disables readiness waiting)
 * \return 0 on success, -1 for a negative value
 *
 * Buffer reads wait for readability before recv() when this timeout is positive.
 * A 0 return from a TCP read may also indicate EOF. read_line() does not use
 * this timeout. The setter does not require an open socket.
 */
int socket::set_read_timeout(int timeout_ms) {
    if (timeout_ms < 0) {
        this->error_code = err_invalid_argument;
        return -1;
    }
    this->read_timeout_ms = timeout_ms;
    this->error_code = err_none;  // Clear previous errors on success
    return 0;
}

/**
 * Set write timeout
 * \param timeout_ms Timeout in milliseconds (0 disables readiness waiting)
 * \return 0 on success, -1 for a negative value
 *
 * The base TCP explicit-size buffer write overloads wait for writability before
 * send(). C-string and std::string overloads send directly. The timeout does
 * not guarantee that an entire buffer is sent within the configured interval.
 */
int socket::set_write_timeout(int timeout_ms) {
    if (timeout_ms < 0) {
        this->error_code = err_invalid_argument;
        return -1;
    }
    this->write_timeout_ms = timeout_ms;
    this->error_code = err_none;  // Clear previous errors on success
    return 0;
}

/**
 * Set connect timeout
 * \param timeout_ms Timeout in milliseconds (0 uses native connect behavior)
 * \return 0 on success, -1 for a negative value
 *
 * A positive TCP connect timeout uses a temporary nonblocking connect,
 * waits for writability, and checks SO_ERROR before completing. Blocking mode
 * is restored after the attempt.
 */
int socket::set_connect_timeout(int timeout_ms) {
    if (timeout_ms < 0) {
        this->error_code = err_invalid_argument;
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
        this->error_code = internal::translate_native_error(GET_NET_SOCKET_ERROR());
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
        this->error_code = internal::translate_native_error(GET_NET_SOCKET_ERROR());
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
        this->error_code = internal::translate_native_error(GET_NET_SOCKET_ERROR());
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
        this->error_code = internal::translate_native_error(GET_NET_SOCKET_ERROR());
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
    
    int result = fcntl(this->sockfd, F_SETFL,
                       nonblocking ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK));
#endif

    if (result != 0) {
        this->error_code = internal::translate_native_error(GET_NET_SOCKET_ERROR());
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

bool socket::tcp_connect(const char* hostname, int port) {
    if (!hostname || !*hostname || !is_valid_port(port)) {
        this->error_code = err_no_socket;
        this->error_state = state_connect;
        return false;
    }
    this->set_remote_hostname(hostname);
    this->port = port;
    return tcp_connect();
}

bool socket::tcp_connect() {
    const std::string hostname = this->remote_hostname();
    if (hostname.empty() || !is_valid_port(this->port)) {
        this->error_code = err_no_host;
        this->error_state = state_resolve;
        return false;
    }
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* results = nullptr;
    const std::string port_string = std::to_string(this->port);
    if (::getaddrinfo(hostname.c_str(), port_string.c_str(),
                      &hints, &results) != 0) {
        this->error_code = err_no_host;
        this->error_state = state_resolve;
        return false;
    }
    bool success = false;
    for (addrinfo* entry = results; entry; entry = entry->ai_next) {
        if (entry->ai_family != AF_INET && entry->ai_family != AF_INET6) continue;
        if (entry->ai_family == AF_INET) {
            success = tcp_connect(address(*reinterpret_cast<sockaddr_in*>(entry->ai_addr)));
        } else {
            success = tcp_connect(address(*reinterpret_cast<sockaddr_in6*>(entry->ai_addr)));
        }
        if (success) break;
    }
    ::freeaddrinfo(results);
    this->connected = success;
    return success;
}

bool socket::tcp_connect(const address& addr) {
    const sockaddr_storage target = addr.get_sockaddr_storage();
    const int family = target.ss_family;
    if (family != AF_INET && family != AF_INET6) {
        this->error_code = err_no_socket;
        this->error_state = state_connect;
        return false;
    }
    const socklen_t length = family == AF_INET6
        ? static_cast<socklen_t>(sizeof(sockaddr_in6))
        : static_cast<socklen_t>(sizeof(sockaddr_in));

    // A fresh descriptor for each attempt prevents stale TCP state on retry.
    this->close();
    this->sockfd = ::socket(family, SOCK_STREAM, 0);
    this->connected = false;
    if (!this->is_valid_socket()) {
        this->error_code = err_no_socket;
        this->error_state = state_create;
        return false;
    }
    this->net_family = family;
    this->configure_socket_for_connect();

    bool succeeded = false;
    if (this->connect_timeout_ms > 0) {
        blocking_mode_guard guard(this);
        if (!guard.is_valid()) {
            this->error_code = internal::translate_native_error(GET_NET_SOCKET_ERROR());
            this->error_state = state_connect;
        } else {
            const int status = ::connect(this->sockfd,
                reinterpret_cast<const sockaddr*>(&target), length);
            if (status == 0) {
                succeeded = true;
            } else {
                const int error = GET_NET_SOCKET_ERROR();
#ifdef NET_PLATFORM_WINDOWS
                const bool pending = error == WSAEWOULDBLOCK || error == WSAEINPROGRESS;
#else
                const bool pending = error == EINPROGRESS || error == EWOULDBLOCK;
#endif
                if (pending) {
                    const int ready = socket::wait_for_writable(this->sockfd,
                        this->connect_timeout_ms);
                    if (ready > 0) {
                        int so_error = 0;
#ifdef NET_PLATFORM_WINDOWS
                        int len = static_cast<int>(sizeof(so_error));
#else
                        socklen_t len = sizeof(so_error);
#endif
                        if (::getsockopt(this->sockfd, SOL_SOCKET, SO_ERROR,
                            reinterpret_cast<char*>(&so_error), &len) != 0) {
                            this->error_code = internal::translate_native_error(GET_NET_SOCKET_ERROR());
                            this->error_state = state_connect;
                        } else if (so_error == 0) {
                            succeeded = true;
                        } else {
                            this->error_code = internal::translate_native_error(so_error);
                            this->error_state = state_connect;
                        }
                    } else if (ready == 0) {
this->error_code = err_timeout;
                        this->error_state = state_connect;
                    } else {
                        this->error_code = internal::translate_native_error(GET_NET_SOCKET_ERROR());
                        this->error_state = state_connect;
                    }
                } else {
                    this->error_code = internal::translate_native_error(error);
                    this->error_state = state_connect;
                }
            }
        }
    } else {
        if (::connect(this->sockfd,
            reinterpret_cast<const sockaddr*>(&target), length) == 0) {
            succeeded = true;
        } else {
            this->error_code = internal::translate_native_error(GET_NET_SOCKET_ERROR());
            this->error_state = state_connect;
        }
    }

    if (!succeeded) {
        this->close();
        return false;
    }
    this->remote_addr = target;
    this->connected = true;
    this->clear_buffers();
    this->error_code = err_none;
    this->error_state = 0;
    return true;
}

int socket::tcp_bind(const sockaddr* addr, socklen_t length) {
    if (!this->is_valid_socket() || !addr) {
        this->error_code = err_no_socket;
        this->error_state = state_bind;
        return -1;
    }
    if (::bind(this->sockfd, addr, length) != 0) {
        this->error_code = internal::translate_native_error(GET_NET_SOCKET_ERROR());
        this->error_state = state_bind;
        return -1;
    }
    this->error_code = err_none;
    return 0;
}

bool socket::tcp_listen(const char* ip, int port) {
    if (!ip || !is_valid_port(port)) {
        this->error_code = err_no_socket;
        this->error_state = state_bind;
        return false;
    }
    sockaddr_storage target{};
    socklen_t length = 0;
    auto* ip4 = reinterpret_cast<sockaddr_in*>(&target);
    auto* ip6 = reinterpret_cast<sockaddr_in6*>(&target);
    if (::inet_pton(AF_INET6, ip, &ip6->sin6_addr) == 1) {
        ip6->sin6_family = AF_INET6;
        ip6->sin6_port = htons(static_cast<unsigned short>(port));
        length = static_cast<socklen_t>(sizeof(sockaddr_in6));
    } else if (::inet_pton(AF_INET, ip, &ip4->sin_addr) == 1) {
        ip4->sin_family = AF_INET;
        ip4->sin_port = htons(static_cast<unsigned short>(port));
        length = static_cast<socklen_t>(sizeof(sockaddr_in));
    } else {
        this->error_code = err_no_socket;
        this->error_state = state_bind;
        return false;
    }
    // Listening is always a new binding attempt. Never reuse a previously
    // listening descriptor, including when rebinding within the same family.
    this->close();
    this->sockfd = ::socket(target.ss_family, SOCK_STREAM, 0);
    if (!this->is_valid_socket()) {
        this->error_code = err_no_socket;
        this->error_state = state_create;
        return false;
    }
    this->net_family = target.ss_family;
    this->port = port;
    this->set_so_reuseaddr(true);
    if (target.ss_family == AF_INET6) this->set_ipv6_v6only(true);
    if (tcp_bind(reinterpret_cast<const sockaddr*>(&target), length) != 0) {
        this->close();
        return false;
    }
    if (::listen(this->sockfd, SOMAXCONN) != 0) {
        this->error_code = internal::translate_native_error(GET_NET_SOCKET_ERROR());
        this->error_state = state_accept;
        this->close();
        return false;
    }
    this->connected = true;
    this->error_state = 0;
    this->error_code = err_none;
    return true;
}

socket_t socket::tcp_accept(int timeout_ms, bool blocking,
                         sockaddr_storage& peer) {
    if (!this->is_valid_socket()) {
        this->error_code = err_no_socket;
        this->error_state = state_accept;
        return invalid_socket;
    }
    if (timeout_ms > 0) {
        const int ready = socket::wait_for_readable(this->sockfd, timeout_ms);
        if (ready == 0) return invalid_socket; // timeout is not an error
        if (ready < 0) {
            this->error_code = internal::translate_native_error(GET_NET_SOCKET_ERROR());
            this->error_state = state_accept;
            return invalid_socket;
        }
    }
    socklen_t length = static_cast<socklen_t>(sizeof(peer));
    const socket_t fd = ::accept(this->sockfd,
        reinterpret_cast<sockaddr*>(&peer), &length);
    if (fd == invalid_socket) {
        const int error = GET_NET_SOCKET_ERROR();
#ifdef NET_PLATFORM_WINDOWS
        if (error == WSAEWOULDBLOCK || error == WSAECONNRESET) return invalid_socket;
#else
        if (error == EAGAIN || error == EWOULDBLOCK || error == ECONNRESET) return invalid_socket;
#endif
        this->error_code = internal::translate_native_error(error);
        this->error_state = state_accept;
        return invalid_socket;
    }
    // Configure the accepted descriptor before transferring ownership.
#ifdef NET_PLATFORM_MACOS
    const int no_sigpipe = 1;
    if (::setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE,
                     reinterpret_cast<const char*>(&no_sigpipe), sizeof(no_sigpipe)) != 0) {
        this->error_code = internal::translate_native_error(GET_NET_SOCKET_ERROR());
        this->error_state = state_accept;
        NET_CLOSE_SOCKET(fd);
        return invalid_socket;
    }
#endif
#ifdef NET_PLATFORM_WINDOWS
    u_long mode = blocking ? 0UL : 1UL;
    if (::ioctlsocket(fd, FIONBIO, &mode) != 0) {
        this->error_code = internal::translate_native_error(GET_NET_SOCKET_ERROR());
        this->error_state = state_accept;
        NET_CLOSE_SOCKET(fd);
        return invalid_socket;
    }
#else
    const int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0 || ::fcntl(fd, F_SETFL,
                           blocking ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK)) != 0) {
        this->error_code = internal::translate_native_error(GET_NET_SOCKET_ERROR());
        this->error_state = state_accept;
        NET_CLOSE_SOCKET(fd);
        return invalid_socket;
    }
#endif
    int nodelay = 1;
    (void)::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY,
                       reinterpret_cast<const char*>(&nodelay), sizeof(nodelay));
    this->error_code = err_none;
    this->error_state = 0;
    return fd;
}

} // aeon