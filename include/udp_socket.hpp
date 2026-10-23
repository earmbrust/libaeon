/*********************************************************************
 * libaeon - A simple, lightweight, cross platform networking library
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/
#pragma once
#include "socket.hpp"

namespace aeon {
class udp_server_socket;
class udp_server_event_socket;

class LIBAEON_API udp_socket : public socket {
public:
    udp_socket();
    ~udp_socket() override;
    bool close() override;
    bool is_valid_socket() const override;

    int write(char* data, int size) override;
    int write(const char* data, int size) override;
    int write(char* data) override;
    int write(const char* data) override;
    int write(const std::string& data) override;
    int read(char* buffer, int size) override;
    std::string read(int size) override;
    int read() override;

    // A logical UDP peer is a udp_socket owned by its server, has no native
    // descriptor of its own, and never keeps its parent server alive.
    udp_socket* parent_socket() const noexcept { return parent_; }

protected:
    udp_socket(udp_socket* parent, const address& endpoint);

private:
    friend class udp_server_socket;
    friend class udp_server_event_socket;
    udp_socket* parent_ = nullptr; // server-owned lifetime makes this safe
    bool peer_active_ = true;
    int send_datagram(const char* data, int size);
};
} // namespace aeon
