/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#pragma once

#include "socket.hpp"

namespace aeon {

    class LIBAEON_API socket_set {
    public:
        socket_set();
        ~socket_set();
        // Preserve the legacy value semantics: copies receive independent
        // non-owning registration lists, never duplicate socket ownership.
        socket_set(const socket_set&);
        socket_set& operator=(const socket_set&);

        bool add(socket* sock_ref);
        bool remove(unsigned int index);
        bool remove(unsigned int index, unsigned int count);

        int size() const;
        socket* at(unsigned int index) const;
        socket* operator[](unsigned int index) const { return at(index); }
        // Number of peer sockets to which the whole payload was written.
        // Returns -1 for invalid arguments; individual failures do not stop others.
        int broadcast(const char* bytes, int count) const;
        int broadcast(const std::string& bytes) const;

        class iterator {
        public:
            iterator(const socket_set* owner, unsigned int index) noexcept
                : owner_(owner), index_(index) {}
            socket* operator*() const { return owner_->at(index_); }
            iterator& operator++() noexcept { ++index_; return *this; }
            bool operator!=(const iterator& rhs) const noexcept {
                return owner_ != rhs.owner_ || index_ != rhs.index_;
            }
        private:
            const socket_set* owner_;
            unsigned int index_;
        };
        iterator begin() const noexcept { return iterator(this, 0); }
        iterator end() const noexcept { return iterator(this, static_cast<unsigned int>(size())); }

    private:
        // Opaque, non-owning registry storage. Constructed and destroyed
        // inside Aeon so STL allocation remains in the library module.
        struct state;
        state* state_;
    };

} // aeon
