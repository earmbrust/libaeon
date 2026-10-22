#pragma once

#include "socket.hpp"
#include <memory>

namespace aeon {

class LIBAEON_API server_socket : public socket {
public:
    server_socket();
    virtual ~server_socket();

    int set_accept_timeout(int timeout_ms);
    int bind(const sockaddr* addr, socklen_t addrlen);
    bool listen();
    bool listen(int port);
    bool listen(const char* address, int port);

    // Accepted connections own exactly one native descriptor; plain servers
    // do not introduce event state or hidden polling threads.
    std::unique_ptr<socket> accept();
    std::unique_ptr<socket> accept(bool blocking);

private:
    int accept_timeout_ms = 0;
};

} // namespace aeon
