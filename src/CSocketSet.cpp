/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#ifndef _CSOCKET_SET_CPP
#define _CSOCKET_SET_CPP

#include "libaeon.h"

namespace net {

/**
 * Add an existing socket to the socket set
 * \param socket_ref Pointer to a CSocket object to add
 * \return true if successful, false otherwise
 * 
 * Add() allows you to add an existing socket to the socket set
 * so that it may be handled as a member of the class.
 */
bool CSocketSet::Add(CSocket* socket_ref) {
    if (!socket_ref) {
        return false;
    }
    this->Sockets.push_back(socket_ref);
    return true;
}

/**
 * Add a new empty socket to the socket set
 * \return true if successful, false otherwise
 * 
 * Deprecated: Creating sockets without parameters is not recommended.
 * Use Add(CSocket* socket_ref) instead.
 */
bool CSocketSet::Add() {
    CSocket* socket_ref = new CSocket();
    if (!socket_ref) {
        return false;
    }
    this->Sockets.push_back(socket_ref);
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
bool CSocketSet::Remove(unsigned int index) {
    // Safe bounds checking
    if (index >= static_cast<unsigned int>(this->Sockets.size())) {
        return false;
    }
    
    this->Sockets.erase(this->Sockets.begin() + index);
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
bool CSocketSet::Remove(unsigned int index, unsigned int count) {
    // Safe bounds checking
    if (count == 0 || index >= static_cast<unsigned int>(this->Sockets.size())) {
        return false;
    }
    
    unsigned int safe_count = count;
    
    // Ensure we don't try to remove more elements than exist
    if ((index + count) > static_cast<unsigned int>(this->Sockets.size())) {
        safe_count = static_cast<unsigned int>(this->Sockets.size() - index);
    }
    
    this->Sockets.erase(this->Sockets.begin() + index, 
                       this->Sockets.begin() + index + safe_count);
    return true;
}

/**
 * Get the number of sockets in the set
 * \return Number of sockets currently in the set
 */
int CSocketSet::Size() {
    return static_cast<int>(this->Sockets.size());
}

}  // namespace net

#endif  // _CSOCKET_SET_CPP