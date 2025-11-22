/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#ifndef _CEVENTSOCKET_SET_CPP
#define _CEVENTSOCKET_SET_CPP

#include "libaeon.h"

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

namespace net {

/**
 * Add an existing event socket to the socket set
 * \param socket_ref Pointer to a CEventSocket object to add
 * \return true if successful, false otherwise
 * 
 * Add() allows you to add an existing event socket to the socket set
 * so that it may be polled and handled as a member of the class.
 */
bool CEventSocketSet::Add(CEventSocket* socket_ref) {
    if (!socket_ref) {
        return false;
    }
    this->Sockets.push_back(socket_ref);
    return true;
}

/**
 * Add a new empty event socket to the socket set
 * \return true if successful, false otherwise
 * 
 * Deprecated: Creating sockets without parameters is not recommended.
 * Use Add(CEventSocket* socket_ref) instead.
 */
bool CEventSocketSet::Add() {
    CEventSocket* socket_ref = new CEventSocket();
    if (!socket_ref) {
        return false;
    }
    this->Sockets.push_back(socket_ref);
    return true;
}

/**
 * Remove a single event socket from the set by index
 * \param index The index of the socket to remove
 * \return true if successful, false otherwise
 * 
 * Remove() allows you to remove an event socket from the socket set.
 * This function does NOT close the socket or free memory - 
 * those tasks should be done prior to removal.
 */
bool CEventSocketSet::Remove(unsigned int index) {
    // Safe bounds checking
    if (index >= static_cast<unsigned int>(this->Sockets.size())) {
        return false;
    }

    this->Sockets.erase(this->Sockets.begin() + index);
    return true;
}

/**
 * Remove multiple event sockets from the set
 * \param index The starting index of sockets to remove
 * \param count The number of sockets to remove
 * \return true if successful, false otherwise
 * 
 * Remove() allows you to remove multiple event sockets from the socket set.
 * This function does NOT close the sockets or free memory - 
 * those tasks should be done prior to removal.
 */
bool CEventSocketSet::Remove(unsigned int index, unsigned int count) {
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
int CEventSocketSet::Size() {
    return static_cast<int>(this->Sockets.size());
}

/**
 * Poll all event sockets in the set
 * 
 * Poll() calls Poll() on each event socket in the set.
 * This triggers the OnRead/OnWrite callbacks for each socket
 * that has data available or is ready to write.
 */
void CEventSocketSet::Poll() {
    // Use range-based for loop for cleaner iteration
    for (auto socket : this->Sockets) {
        if (socket) {
            socket->Poll();
        }
    }
}

/**
 * Clean up and close all sockets in the set
 * 
 * Cleanup() closes all sockets in the set, frees their memory,
 * and clears the socket list. Use this when shutting down.
 */
void CEventSocketSet::Cleanup() {
    // Close and delete all sockets
    for (auto socket : this->Sockets) {
        if (socket) {
            socket->Close();
            delete socket;
        }
    }
    // Clear the vector
    this->Sockets.clear();
}

}  // namespace net

#endif  // _CEVENTSOCKET_SET_CPP