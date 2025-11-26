/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#include <aeon.hpp>

namespace aeon {

/**
 * Add an existing socket to the socket set
 * \param socket_ref Pointer to a socket object to add
 * \return true if successful, false otherwise
 * 
 * Add() allows you to add an existing socket to the socket set
 * so that it may be handled as a member of the class.
 */
bool socket_set::add(socket* socket_ref) {
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
 * Remove a single socket from the set by index
 * \param index The index of the socket to remove
 * \return true if successful, false otherwise
 * 
 * Remove() allows you to remove a socket from the socket set.
 * This function does NOT close the socket or free memory - 
 * those tasks should be done prior to removal.
 */
bool socket_set::remove(unsigned int index) {
    // Safe bounds checking
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
 * Remove multiple sockets from the set
 * \param index The starting index of sockets to remove
 * \param count The number of sockets to remove
 * \return true if successful, false otherwise
 * 
 * Remove() allows you to remove multiple sockets from the socket set.
 * This function does NOT close the sockets or free memory - 
 * those tasks should be done prior to removal.
 */
bool socket_set::remove(unsigned int index, unsigned int count) {
    // Safe bounds checking
    if (count == 0 || index >= static_cast<unsigned int>(this->sockets_.size())) {
        this->error_code_ = err_no_socket;
        this->error_state_ = state_accept;
        return false;
    }
    
    unsigned int safe_count = count;
    
    // Ensure we don't try to remove more elements than exist
    if ((index + count) > static_cast<unsigned int>(this->sockets_.size())) {
        safe_count = static_cast<unsigned int>(this->sockets_.size() - index);
    }
    
    this->sockets_.erase(this->sockets_.begin() + index, 
                       this->sockets_.begin() + index + safe_count);
    this->error_code_ = err_none;
    this->error_state_ = 0;
    return true;
}

/**
 * Get the number of sockets in the set
 * \return Number of sockets currently in the set
 */
int socket_set::size() const {
    return static_cast<int>(this->sockets_.size());
}

}  // aeon