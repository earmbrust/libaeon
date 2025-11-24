/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#include <net.h>

namespace net {

/**
 * Add an existing event socket to the socket set
 * \param socket_ref Pointer to a event_socket object to add
 * \return true if successful, false otherwise
 * 
 * Add() allows you to add an existing event socket to the socket set
 * so that it may be polled and handled as a member of the class.
 */
bool event_socket_set::add(event_socket* socket_ref) {
    if (!socket_ref) {
        this->error_code_ = err_no_socket;
        this->error_state_ = state_create;
        return false;
    }
    this->sockets_.push_back(socket_ref);
    this->error_code_ = err_none;
    this->error_state_ = 0;
    return true;
}

/**
 * Remove a single event socket from the set by index
 * \param index The index of the socket to remove
 * \return true if successful, false otherwise
 * 
 * Remove() allows you to remove a socket from the socket set.
 * This function does NOT close the socket or free memory - 
 * those tasks should be done prior to removal.
 */
bool event_socket_set::remove(unsigned int index) {
    if (index >= static_cast<unsigned int>(this->sockets_.size())) {
        this->error_code_ = err_no_socket;
        this->error_state_ = state_accept;
        return false;
    }

    this->sockets_.erase(this->sockets_.begin() + index);
    this->error_code_ = err_none;
    this->error_state_ = 0;
    return true;
}

/**
 * Remove multiple event sockets from the set
 * \param index The starting index of sockets to remove
 * \param count The number of sockets to remove
 * \return true if successful, false otherwise
 */
bool event_socket_set::remove(unsigned int index, unsigned int count) {
    if (count == 0 || index >= static_cast<unsigned int>(this->sockets_.size())) {
        this->error_code_ = err_no_socket;
        this->error_state_ = state_accept;
        return false;
    }

    unsigned int safe_count = count;
    if ((index + count) > static_cast<unsigned int>(this->sockets_.size())) {
        safe_count = static_cast<unsigned int>(this->sockets_.size()) - index;
    }

    this->sockets_.erase(this->sockets_.begin() + index,
                        this->sockets_.begin() + index + safe_count);
    this->error_code_ = err_none;
    this->error_state_ = 0;
    return true;
}

/**
 * Get the number of event sockets in the set
 * \return Number of event sockets currently in the set
 */
int event_socket_set::size() const {
    return static_cast<int>(this->sockets_.size());
}

/**
 * Poll all sockets in the set for read/write events
 * This triggers the on_read/on_write callbacks for each socket
 */
void event_socket_set::poll() {
    for (unsigned int i = 0; i < static_cast<unsigned int>(this->sockets_.size()); ++i) {
        if (this->sockets_[i]) {
            this->sockets_[i]->poll();
        }
    }
}

/**
 * Clean up and close all sockets in the set
 */
void event_socket_set::cleanup() {
    for (unsigned int i = 0; i < static_cast<unsigned int>(this->sockets_.size()); ++i) {
        if (this->sockets_[i]) {
            this->sockets_[i]->close();
            delete this->sockets_[i];
        }
    }
    this->sockets_.clear();
    this->error_code_ = err_none;
    this->error_state_ = 0;
}

}  // namespace net