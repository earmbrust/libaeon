/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#pragma once

#ifdef _MSC_VER
#pragma warning(disable:4251)
#endif

#include "address.h"
#include "internal/config.h"
#include "internal/platform.h"
#include "internal/export.h"

namespace net {

    /**
     * \brief RAII guard for temporary non-blocking socket mode
     * 
     * Automatically restores original blocking mode when destroyed,
     * even if an error occurs.
     */
    class LIBAEON_API blocking_mode_guard {
    private:
        class socket* socket_;
        bool original_blocking_;
        bool valid_;

    public:
        explicit blocking_mode_guard(class socket* sock);
        ~blocking_mode_guard();

        bool is_valid() const { return valid_; }

        blocking_mode_guard(const blocking_mode_guard&) = delete;
        blocking_mode_guard(blocking_mode_guard&&) = delete;
        blocking_mode_guard& operator=(const blocking_mode_guard&) = delete;
        blocking_mode_guard& operator=(blocking_mode_guard&&) = delete;
    };

    /**
     * \class socket
     * \brief Generic socket implementation
     * 
     * Base socket class that can be inherited to extend functionality.
     */
    class LIBAEON_API socket {
    public:
        friend class blocking_mode_guard;

        // Constants
        static constexpr int max_buffer_size = 256;
        static constexpr int stream_type = SOCK_STREAM;
        static constexpr int datagram_type = SOCK_DGRAM;
        static constexpr int default_type = socket::stream_type;
        static constexpr int family_ipv4 = AF_INET;
        static constexpr int family_ipv6 = AF_INET6;
        static constexpr int default_family = socket::family_ipv4;

        // Configuration methods
        int set_blocking(bool flag);
        int set_read_timeout(int timeout_ms);
        int set_write_timeout(int timeout_ms);
        int set_connect_timeout(int timeout_ms);
        int set_tcp_nodelay(bool enabled);
        int set_so_reuseaddr(bool enabled);
        int set_so_linger(u_short linger_time_sec);
        int set_ipv6_v6only(bool enabled);

        // I/O methods
        virtual int write(char* data, int size);
        virtual int write(const char* data, int size);
        virtual int write(char* data);
        virtual int write(const char* data);
        virtual int write(const std::string& data);

        virtual int read();
        virtual int read(char* buffer, int size);
        virtual int read_line(char* buffer, int size);
        virtual int read_until(char* buffer, int size);
        virtual std::string read(int size);

        // Socket configuration shortcuts
        void set_socket_reuseaddr();
        void set_socket_tcp_nodelay();
        void set_socket_linger(u_short linger_sec);

        // Lifecycle
        socket();
        explicit socket(int family_type);
        socket(int family, int type);
        socket(socket_t existing_fd, bool is_existing_socket);
        virtual ~socket();

        bool close();
        bool is_valid_socket() const;

        // Accessors
        std::string get_remote_ip() const;
        int get_remote_port() const;
        address get_remote_address() const;

        int get_error() const;
        int get_state() const;

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
        char inbuffer[socket::max_buffer_size];
        char outbuffer[socket::max_buffer_size];
        std::string remote_host;
        std::string remote_ip;
        int connect_code;
        int error_code;
        int error_state;
        int port;
        int read_timeout_ms;
        int write_timeout_ms;
        int connect_timeout_ms;

        void set_error(int error);
        void clear_buffers();
        void clear_buffer(char* buffer, int size);

        void configure_socket_for_connect();
        int set_socket_nonblocking(bool nonblocking);
        void safe_clear_buffer(char* buffer, std::size_t size);

        static int wait_for_readable(socket_t sockfd, int timeout_ms);
        static int wait_for_writable(socket_t sockfd, int timeout_ms);

#ifdef NET_PLATFORM_WINDOWS
        int wsaret;
        WSADATA wsadata;
#endif
    };

} // namespace net