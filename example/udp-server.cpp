/******************************************************************
 * udp-server.cpp - A simple UDP echo server using libaeon
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#include <net.h>
#include <iostream>
#include <cstdio>

#define SERVER_PORT 2301

int main(void) {
    std::cout << "Starting UDP server on port " << SERVER_PORT << std::endl;

    // Create UDP server socket
    net::udp_server_socket server;

    // Bind to the specified port
    if (!server.listen(SERVER_PORT)) {
        std::fprintf(stderr, "Error: Failed to bind to port %d\n", SERVER_PORT);
        return EXIT_FAILURE;
    }

    std::cout << "UDP server listening on port " << SERVER_PORT << std::endl;
    std::cout << "Waiting for datagrams...\n";

    char buffer[256];
    int bytes_read = 0;
    int message_count = 0;

    // Main server loop
    while (true) {
        // Read datagram from client
        bytes_read = server.read(buffer, sizeof(buffer) - 1);

        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            ++message_count;

            // Use accessor methods - they handle IPv4/IPv6 casting internally
            std::printf("Message %d from %s:%d: %s\n",
                       message_count,
                       server.get_remote_ip().c_str(),
                       server.get_remote_port(),
                       buffer);

            // Send response back to client
            std::string response = "Server received: ";
            response += buffer;
            int bytes_sent = server.write(response.c_str(), response.size());
            
            if (bytes_sent > 0) {
                std::printf("Sent response to client (%d bytes)\n", bytes_sent);
            } else {
                std::fprintf(stderr, "Error: Failed to send response\n");
            }
        } else if (bytes_read < 0) {
            std::fprintf(stderr, "Error: Failed to read datagram\n");
        }
    }

    return EXIT_SUCCESS;
}