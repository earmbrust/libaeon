/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/


#include <net.h>
#include <cstring>

namespace net {


/**
 * Poll for incoming data on the socket
 * \return The result of OnRead() callback
 * 
 * Poll() reads available data from the socket into the internal buffer,
 * then calls the OnRead() member function with the data.
 * Override OnRead() in derived classes to handle incoming data.
 */
bool event_socket::poll() {
    if (!this->is_valid_socket()) {
        return false;
    }

    this->safe_clear_buffer(this->inbuffer, socket::max_buffer_size);
    int bytesRead = recv(this->sockfd, this->inbuffer, socket::max_buffer_size, 0);
    this->n = bytesRead;

    // Handle error and disconnect cases - don't pass negative values to callback
    if (bytesRead <= 0) {
        if (bytesRead < 0) {
            this->error_code = GET_NET_SOCKET_ERROR();
            this->error_state = state_accept;
        } else {
            this->connected = false;
        }
        // Don't process on error or disconnect
        return this->on_read(nullptr, 0);
    }

    // Call the OnRead callback with valid data
    return this->on_read(this->inbuffer, bytesRead);
}

/**
 * Write character data to the socket
 * \param data Pointer to null-terminated string to write
 * \return The number of bytes written
 * 
 * Sends data to the socket and calls the OnWrite() callback
 */
int event_socket::write(char* data) {
    if (!data || !this->is_valid_socket()) {
        return NET_SOCKET_ERROR;
    }

    std::size_t len = std::strlen(data);
    if (len == 0) {
        return 0;
    }

    int bytesSent = send(this->sockfd, data, static_cast<int>(len), 0);
    
    if (bytesSent < 0) {
        this->error_code = GET_NET_SOCKET_ERROR();
        this->error_state = state_connect;
        // Call callback with 0 bytes sent on error, not -1
        this->on_write(data, static_cast<int>(len), 0);
        return bytesSent;
    }

    // Call the OnWrite callback with actual bytes sent
    this->on_write(data, static_cast<int>(len), bytesSent);
    return bytesSent;
}

/**
 * Write const character data to the socket
 * \param data Pointer to null-terminated const string to write
 * \return The number of bytes written
 */
int event_socket::write(const char* data) {
    return this->write(const_cast<char*>(data));
}

/**
 * Write std::string data to the socket
 * \param data std::string to write
 * \return The number of bytes written
 * 
 * Sends string data to the socket and calls the OnWrite() callback
 */
int event_socket::write(const std::string& data) {
    if (!this->is_valid_socket() || data.empty()) {
        return NET_SOCKET_ERROR;
    }

    int bytesSent = send(this->sockfd, data.c_str(), static_cast<int>(data.size()), 0);
    
    if (bytesSent < 0) {
        this->error_code = GET_NET_SOCKET_ERROR();
        this->error_state = state_connect;
        // Call callback with 0 bytes sent on error, not -1
        this->on_write(data.c_str(), static_cast<int>(data.size()), 0);
        return bytesSent;
    }

    // Call the OnWrite callback with actual bytes sent
    this->on_write(data.c_str(), static_cast<int>(data.size()), bytesSent);
    return bytesSent;
}

/**
 * Called when data is received (virtual callback)
 * \param buffer Pointer to data buffer
 * \param size Number of bytes received
 * \return true to continue, false to stop polling
 * 
 * Override this method in derived classes to handle incoming data.
 * Return false to stop the polling mechanism.
 */
bool event_socket::on_read(const char* buffer, int size) {
    (void)buffer;  // Suppress unused parameter warning
    (void)size;
    return false;
}

/**
 * Called when data is sent (virtual callback)
 * \param buffer Pointer to data that was sent
 * \param size Number of bytes requested to send
 * \param sentsize Number of bytes actually sent
 * 
 * Override this method in derived classes to handle send completion.
 */
void event_socket::on_write(const char* buffer, int size, int sentsize) {
    (void)buffer;       // Suppress unused parameter warning
    (void)size;
    (void)sentsize;
}

}  // namespace net