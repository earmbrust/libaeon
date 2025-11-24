/******************************************************************
 * simple-server.cpp - Event-driven server using CEventSocket
 * Demonstrates the proper way to use libaeon for medical-grade resilience
 * No timeouts, no polling loops, just event-driven I/O with callbacks
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#include <libaeon.h>
#include <iostream>
#include <cstdio>
#include <csignal>
#include <atomic>
#include <memory>
#include <thread>
#include <chrono>

#define SERVER_PORT 2300

static std::atomic<bool> shutdown_requested(false);

void signal_handler(int sig) {
    if (sig == SIGINT) {
        std::cout << "\n*** Shutdown requested...\n";
        shutdown_requested = true;
    }
}

int main(void) {
    std::cout << "Event-driven server using CEventSocket\n";
    std::cout << "Listening on port " << SERVER_PORT << "\n";
    std::cout << "Press Ctrl-C to exit.\n";

    if (std::signal(SIGINT, signal_handler) == SIG_ERR) {
        std::cout << "Error: Cannot create signal handler.\n";
        return EXIT_FAILURE;
    }

    net::CServerSocket server;

    if (!server.Listen(SERVER_PORT)) {
        std::fprintf(stderr, "Error: Failed to listen on port %d\n", SERVER_PORT);
        return EXIT_FAILURE;
    }

    std::cout << "Server listening...\n";

    // Make Accept() non-blocking - returns immediately if no pending connections
    // This allows the main loop to check shutdown_requested without artificial timeouts
    server.SetBlocking(false);
    
    net::CEventSocketSet client_sockets;
    int connection_count = 0;

    // Main server loop - event-driven
    while (!shutdown_requested) {
        // Accept new connections - returns unique_ptr, transfer ownership to set
        auto client = server.Accept();
        
        if (client && client->connected) {
            ++connection_count;
            std::printf("Client %d accepted\n", connection_count);
            
            // Configure socket
            client->SetTCPNodelay(true);
            
            std::printf("About to send greeting - client->connected=%d, sockfd=%d\n", client->connected, (int)client->sockfd);

            // Send greeting
            int bytes_sent = client->Write("Hello, world!\r\n");
            if (bytes_sent > 0) {
                std::printf("Sent greeting to client %d (%d bytes)\n", 
                           connection_count, bytes_sent);
            }
            
            // Transfer ownership to managed set via release()
            // The set will clean up the pointer in Cleanup()
            client_sockets.Add(client.release());
            
        }
        
        // Poll all connected clients
        client_sockets.Poll();
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Cleanup
    client_sockets.Cleanup();
    server.Close();
    
    std::cout << "Server shutdown complete.\n";
    return EXIT_SUCCESS;
}