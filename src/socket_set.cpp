/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#include <aeon.hpp>
#include <vector>
#include <utility>
#include <climits>

namespace aeon {
namespace internal {
struct socket_set_state {
    std::vector<socket*> sockets;
    int error_code = err_none;
    int error_state = 0;
};
} // namespace internal

socket_set::socket_set() : state_(new internal::socket_set_state) {}
socket_set::socket_set(const socket_set& other)
    : state_(new internal::socket_set_state(*other.state_)) {}
socket_set& socket_set::operator=(const socket_set& other) {
    if (this != &other) {
        // Allocate before replacing the old state, so a throwing vector
        // copy leaves the destination untouched.
        auto* replacement = new internal::socket_set_state(*other.state_);
        std::swap(state_, replacement);
        delete replacement;
    }
    return *this;
}
socket_set::~socket_set() { delete state_; }

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
        state_->error_code = err_no_socket;
        state_->error_state = state_create;
        return false;
    }
    state_->sockets.push_back(socket_ref);
    state_->error_code = err_none;
    state_->error_state = 0;
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
    if (index >= static_cast<unsigned int>(state_->sockets.size())) {
        state_->error_code = err_no_socket;
        state_->error_state = state_accept;
        return false;
    }
    
    state_->sockets.erase(state_->sockets.begin() + index);
    state_->error_code = err_none;
    state_->error_state = 0;
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
    if (count == 0 || index >= static_cast<unsigned int>(state_->sockets.size())) {
        state_->error_code = err_no_socket;
        state_->error_state = state_accept;
        return false;
    }
    
    unsigned int safe_count = count;
    
    // Ensure we don't try to remove more elements than exist
    if (count > static_cast<unsigned int>(state_->sockets.size()) - index) {
        safe_count = static_cast<unsigned int>(state_->sockets.size() - index);
    }
    
    state_->sockets.erase(state_->sockets.begin() + index,
                       state_->sockets.begin() + index + safe_count);
    state_->error_code = err_none;
    state_->error_state = 0;
    return true;
}

/**
 * Get the number of sockets in the set
 * \return Number of sockets currently in the set
 */
int socket_set::size() const {
    return static_cast<int>(state_->sockets.size());
}
socket* socket_set::at(unsigned int index) const {
    return index < state_->sockets.size() ? state_->sockets[index] : nullptr;
}
int socket_set::broadcast(const char* bytes, int count) const {
    if (count < 0 || (count > 0 && !bytes)) return -1;
    int delivered = 0;
    for (auto* peer : state_->sockets) {
        if (peer && peer->write(bytes, count) == count) ++delivered;
    }
    return delivered;
}
int socket_set::broadcast(const std::string& bytes) const {
    if (bytes.size() > static_cast<std::size_t>(INT_MAX)) return -1;
    return broadcast(bytes.data(), static_cast<int>(bytes.size()));
}

}  // aeon