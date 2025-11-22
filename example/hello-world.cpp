/******************************************************************
 * hello-world.cpp - A simple "Hello World" server using libaeon
 * Copyright (c) 2006-2025 Elden Armbrust
 ******************************************************************/

#include <libaeon.h>
#include <iostream>
#include <csignal>
#include <cstdlib>
#include <atomic>

#define PORT 2300

// Global flag for signal handling
static std::atomic<bool> shutdown_requested(false);

// Forward declaration
void signal_handler(int sig);

int main(void) {
    std::cout << "hello-world.cpp - A simple libaeon hello world server\n";
    std::cout << "2007-2025 (c) Elden Armbrust (BSD License)\n";
    std::cout << "Press Ctrl-C to exit.\n";

    // Set up signal handler for ctrl-c
    if (std::signal(SIGINT, signal_handler) == SIG_ERR) {
        std::cout << "Error: Cannot create signal handler.\n";
        return EXIT_FAILURE;
    }

    // Create server socket
    net::CServerSocket server;

    // Check if we can open the port
    if (!server.Listen(PORT)) {
        std::cout << "Error: Failed to listen on port " << PORT << ".\n";
        return EXIT_FAILURE;
    }

    std::cout << "Server listening on port " << PORT << ".\n";

    // Main server loop
    while (!shutdown_requested) {
        // Accept incoming connection
        net::CSocket* client = server.Accept();

        if (client && client->connected) {
            std::cout << "Client accepted.\n";
            
            // Send hello world message
            int bytes_sent = client->Write("Hello world!\n");
            if (bytes_sent > 0) {
                std::cout << "Sent greeting (" << bytes_sent << " bytes).\n";
            } else {
                std::cout << "Error: Failed to send greeting.\n";
            }
            
            // Close the connection
            client->Close();
            delete client;
            std::cout << "Client connection closed.\n";
        } else if (client) {
            delete client;
        }
    }

    std::cout << "Shutdown complete.\n";
    return EXIT_SUCCESS;
}

/**
 * Signal handler for SIGINT (ctrl-c)
 */
void signal_handler(int sig) {
    if (sig == SIGINT) {
        std::cout << "\nGot SIGINT. Shutting down gracefully.\n";
        shutdown_requested = true;
    }
}