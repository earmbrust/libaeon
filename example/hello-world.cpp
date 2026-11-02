/******************************************************************
 * hello-world.cpp - A simple "Hello World" server using libaeon
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#include <aeon.hpp>
#include <iostream>
#include <csignal>
#include <cstdlib>

#define PORT 2300

// Global flag for signal handling
static volatile std::sig_atomic_t shutdown_requested = 0;

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
    aeon::server_socket server;

    // Check if we can open the port
    if (!server.listen(PORT)) {
        std::cout << "Error: Failed to listen on port " << PORT << ".\n";
        return EXIT_FAILURE;
    }

    if (server.set_accept_timeout(200) != 0) {
        std::cout << "Error: Cannot configure accept timeout.\n";
        return EXIT_FAILURE;
    }

    std::cout << "Server listening on port " << PORT << ".\n";

    while (!shutdown_requested) {
        // The listener owns accepted peers; remove each after serving it.
        auto* client = server.accept();
        if (!client) continue;

        if (client->connected) {
            std::cout << "Client accepted.\n";
            const int bytes_sent = client->write("Hello world!\n");
            if (bytes_sent > 0) {
                std::cout << "Sent greeting (" << bytes_sent << " bytes).\n";
            } else {
                std::cout << "Error: Failed to send greeting.\n";
            }
        }

        client->close();
        server.remove_peer(client);
        std::cout << "Client connection closed.\n";
    }

    std::cout << "Shutdown complete.\n";
    return EXIT_SUCCESS;
}

/**
 * Signal handler for SIGINT (ctrl-c)
 */
void signal_handler(int sig) {
    if (sig == SIGINT) shutdown_requested = 1;
}