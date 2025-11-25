/******************************************************************
 * simple-server.cpp - Event-driven server using CEventSocket
 * Demonstrates the proper way to use libaeon for medical-grade resilience
 * No timeouts, no polling loops, just event-driven I/O with callbacks
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#include <net.h>
#include <iostream>
#include <cstdio>
#include <csignal>
#include <atomic>
#include <memory>
#include <thread>
#include <chrono>
#include <cstring>

static std::atomic<bool> shutdown_requested(false);

void signal_handler(int sig) {
    if (sig == SIGINT) {
        std::cout << "\n*** Shutdown requested...\n";
        shutdown_requested = true;
    }
}

void print_usage(const char* program) {
    std::fprintf(stderr, "Usage: %s [address] [port]\n", program);
    std::fprintf(stderr, "Default: %s 0.0.0.0 2300\n", program);
    std::fprintf(stderr, "Examples:\n");
    std::fprintf(stderr, "  %s 0.0.0.0 2300      (IPv4 any)\n", program);
    std::fprintf(stderr, "  %s 127.0.0.1 2300    (IPv4 loopback)\n", program);
    std::fprintf(stderr, "  %s :: 2300           (IPv6 any)\n", program);
    std::fprintf(stderr, "  %s ::1 2300          (IPv6 loopback)\n", program);
}

int main(int argc, char** argv) {
    const char* bind_address = "0.0.0.0";  // Default
    int port = 2300;                        // Default

    // Parse arguments
    if (argc > 1) {
        if (std::strcmp(argv[1], "-h") == 0 || std::strcmp(argv[1], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
        bind_address = argv[1];
    }

    if (argc > 2) {
        port = std::atoi(argv[2]);
        if (port <= 0 || port > 65535) {
            std::fprintf(stderr, "Error: Invalid port %d\n", port);
            return EXIT_FAILURE;
        }
    }

    std::cout << "Event-driven server using CEventSocket\n";
    std::cout << "Binding to " << bind_address << ":" << port << "\n";
    std::cout << "Press Ctrl-C to exit.\n";

    if (std::signal(SIGINT, signal_handler) == SIG_ERR) {
        std::cout << "Error: Cannot create signal handler.\n";
        return EXIT_FAILURE;
    }

    net::server_socket server;

    if (!server.listen(bind_address, port)) {
        std::fprintf(stderr, "Error: Failed to listen on %s:%d\n", bind_address, port);
        std::fprintf(stderr, "Error code: %d\n", server.get_error());
        return EXIT_FAILURE;
    }

    std::cout << "Server listening...\n";

    // Make Accept() non-blocking - returns immediately if no pending connections
    // This allows the main loop to check shutdown_requested without artificial timeouts
    server.set_blocking(false);
    
    net::event_socket_set client_sockets;
    int connection_count = 0;

    // Main server loop - event-driven
    while (!shutdown_requested) {
        // Accept new connections - returns unique_ptr, transfer ownership to set
        auto client = server.accept();
        
        if (client && client->connected) {
            ++connection_count;
            std::printf("Client %d accepted\n", connection_count);
            
            // Configure socket
            client->set_socket_tcp_nodelay();
            
            std::printf("About to send greeting - client->connected=%d, sockfd=%d\n", client->connected, (int)client->sockfd);

            // Send greeting
            int bytes_sent = client->write("Hello, world!\r\n");
            if (bytes_sent > 0) {
                std::printf("Sent greeting to client %d (%d bytes)\n", 
                           connection_count, bytes_sent);
            }
            
            // Transfer ownership to managed set via release()
            // The set will clean up the pointer in Cleanup()
            client_sockets.add(client.release());
            
        }
        
        // Poll all connected clients
        client_sockets.poll();
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Cleanup
    client_sockets.cleanup();
    server.close();
    
    std::cout << "Server shutdown complete.\n";
    return EXIT_SUCCESS;
}