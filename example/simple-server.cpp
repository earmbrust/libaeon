/******************************************************************
 * simple-server.cpp - Event-driven server using server_event_socket
 * Demonstrates the proper way to use libaeon for medical-grade resilience
 * No timeouts, no manual polling - just event-driven I/O with callbacks
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#ifdef _MSC_VER
#pragma warning(disable:4251)
#endif

#include <aeon.hpp>
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

    std::cout << "Event-driven server using server_event_socket\n";
    std::cout << "Binding to " << bind_address << ":" << port << "\n";
    std::cout << "Press Ctrl-C to exit.\n";

    if (std::signal(SIGINT, signal_handler) == SIG_ERR) {
        std::cout << "Error: Cannot create signal handler.\n";
        return EXIT_FAILURE;
    }

    aeon::server_event_socket server;

    if (!server.listen(bind_address, port)) {
        std::fprintf(stderr, "Error: Failed to listen on %s:%d\n", bind_address, port);
        std::fprintf(stderr, "Error code: %d\n", server.server_socket::get_error());
        return EXIT_FAILURE;
    }

    std::cout << "Server listening...\n";

    // Make accept non-blocking
    server.server_socket::set_blocking(false);
    
    aeon::event_socket_set client_sockets;
    int connection_count = 0;

    // Set callbacks for client events
    client_sockets.on_socket_data = [](aeon::event_socket* client, const char* buffer, int size) {
        std::printf("Received %d bytes from client\n", size);
        // Echo back to client
        client->write(buffer);
    };

    client_sockets.on_socket_connect = [](aeon::event_socket* client) {
        std::printf("Client connected\n");
    };

    client_sockets.on_socket_close = [](aeon::event_socket* client) {
        std::printf("Client closed connection\n");
    };

    client_sockets.on_socket_error = [](aeon::event_socket* client, int error_code) {
        std::printf("Client error: %d\n", error_code);
    };

    // Main server loop - event-driven
    while (!shutdown_requested) {
        // Accept new connections
        auto client = server.accept();
        
        if (client) {
            ++connection_count;
            std::printf("Client %d accepted\n", connection_count);
            
            // Configure socket
            client->set_socket_tcp_nodelay();

            // Send greeting
            int bytes_sent = client->write("Hello, world!\r\n");
            if (bytes_sent > 0) {
                std::printf("Sent greeting to client %d (%d bytes)\n", 
                           connection_count, bytes_sent);
            }
            
            // Add to set - event socket will auto-poll in background thread
            client_sockets.add(client.release());
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Cleanup
    server.close();
    
    std::cout << "Server shutdown complete.\n";
    return EXIT_SUCCESS;
}