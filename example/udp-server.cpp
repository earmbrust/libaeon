/******************************************************************
 * udp-server.cpp - A simple UDP echo server using libaeon
 * Copyright (c) 2006-2025 Elden Armbrust
 ******************************************************************/

#include <libaeon.h>
#include <iostream>
#include <cstdio>

#define SERVER_PORT 2301

int main(void) {
    std::cout << "Starting UDP server on port " << SERVER_PORT << std::endl;

    // Create UDP server socket
    net::CServerSocketUDP server;

    // Bind to the specified port
    if (!server.Listen(SERVER_PORT)) {
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
        bytes_read = server.Read(buffer, sizeof(buffer) - 1);

        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            ++message_count;

            std::printf("Message %d from %s:%d: %s\n",
                       message_count,
                       inet_ntoa(server.remote_addr.sin_addr),
                       ntohs(server.remote_addr.sin_port),
                       buffer);

            // Send response back to client
            std::string response = "Server received: ";
            response += buffer;
            int bytes_sent = server.Write(response.c_str(), response.size());
            
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