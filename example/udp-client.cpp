/******************************************************************
 * udp-client.cpp - A simple UDP client using libaeon
 * Copyright (c) 2006-2025 Elden Armbrust
 ******************************************************************/

#include <libaeon.h>
#include <iostream>
#include <cstdio>
#include <cstring>

int main(int argc, char** argv) {
    std::cout << "UDP Client\n";

    if (argc != 3) {
        std::fprintf(stderr, "Usage: %s <hostname> <port>\n", argv[0]);
        std::fprintf(stderr, "Example: %s localhost 2301\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char* hostname = argv[1];
    int port = std::atoi(argv[2]);

    std::cout << "Connecting to " << hostname << ":" << port << std::endl;

    // Create UDP client socket
    net::CClientSocketUDP client(hostname, port);

    if (!client.connected) {
        std::fprintf(stderr, "Error: Failed to set up connection to %s:%d\n", hostname, port);
        return EXIT_FAILURE;
    }

    std::cout << "Ready to send datagrams. Type messages (quit to exit):\n";

    char input_buffer[256];
    char response_buffer[256];

    while (true) {
        // Get user input
        std::cout << "> ";
        std::fflush(stdout);

        if (!std::fgets(input_buffer, sizeof(input_buffer), stdin)) {
            break;
        }

        // Remove trailing newline
        std::size_t len = std::strlen(input_buffer);
        if (len > 0 && input_buffer[len - 1] == '\n') {
            input_buffer[len - 1] = '\0';
            len--;
        }

        if (len == 0) {
            continue;
        }

        // Check for quit command
        if (std::strcmp(input_buffer, "quit") == 0 || std::strcmp(input_buffer, "exit") == 0) {
            std::cout << "Exiting...\n";
            break;
        }

        // Send datagram to server
        int bytes_sent = client.Write(input_buffer);
        if (bytes_sent > 0) {
            std::printf("Sent %d bytes to server\n", bytes_sent);
        } else {
            std::fprintf(stderr, "Error: Failed to send datagram\n");
            continue;
        }

        // Read response from server
        int bytes_read = client.Read(response_buffer, sizeof(response_buffer) - 1);
        if (bytes_read > 0) {
            response_buffer[bytes_read] = '\0';
            std::printf("Server response: %s\n", response_buffer);
        } else {
            std::fprintf(stderr, "Error: No response from server\n");
        }
    }

    return EXIT_SUCCESS;
}