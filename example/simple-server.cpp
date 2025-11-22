/******************************************************************
 * simple-server.cpp - Event-driven server with threading and non-blocking I/O
 * Copyright (c) 2006-2025 Elden Armbrust
 ******************************************************************/

#include <libaeon.h>
#include <iostream>
#include <cstdio>
#include <vector>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>
#include <mutex>

#define SERVER_PORT 2300

// Global flag for signal handling
static std::atomic<bool> shutdown_requested(false);

// Signal handler for ctrl-c
void signal_handler(int sig) {
    if (sig == SIGINT) {
        std::cout << "\n*** Shutdown requested...\n";
        shutdown_requested = true;
    }
}

int main(void) {
    std::cout << "Starting threaded non-blocking server on port " << SERVER_PORT << std::endl;
    std::cout << "Press Ctrl-C to exit.\n";

    // Set up signal handler for ctrl-c
    if (std::signal(SIGINT, signal_handler) == SIG_ERR) {
        std::cout << "Error: Cannot create signal handler.\n";
        return EXIT_FAILURE;
    }

    // Create server socket
    net::CServerSocket server;

    // Listen on the specified port
    if (!server.Listen(SERVER_PORT)) {
        std::fprintf(stderr, "Error: Failed to listen on port %d\n", SERVER_PORT);
        return EXIT_FAILURE;
    }

    // Set accept timeout to 100ms so accept thread wakes up frequently
    server.SetAcceptTimeout(100);

    std::cout << "Server listening... waiting for connections\n";

    // Shared state between threads
    std::vector<net::CSocket*> clients;
    std::vector<net::CSocket*> pending_clients;
    std::mutex pending_lock;
    int connection_count = 0;

    // Thread that accepts connections
    auto accept_thread_func = [&]() {
        std::cout << "[Accept Thread] Started\n";
        
        while (!shutdown_requested) {
            // Accept() will use the 100ms timeout set above
            net::CSocket* client = server.Accept();
            
            if (client && client->connected) {
                // Got a connection
                {
                    std::lock_guard<std::mutex> lock(pending_lock);
                    pending_clients.push_back(client);
                }
                std::cout << "[Accept Thread] New connection queued\n";
            } else if (client) {
                // Accept returned but no connection (timeout or error)
                delete client;
            }
            // else: nullptr returned on timeout - just loop
            
            if (shutdown_requested) break;
        }
        
        std::cout << "[Accept Thread] Exiting\n";
    };

    std::thread acceptor(accept_thread_func);

    // Main server loop
    while (!shutdown_requested) {
        // Check for new connections from accept thread
        {
            std::lock_guard<std::mutex> lock(pending_lock);
            while (!pending_clients.empty()) {
                net::CSocket* client = pending_clients.back();
                pending_clients.pop_back();
                ++connection_count;
                std::printf("Client %d connected\n", connection_count);

                // Send greeting message to client
                int bytes_sent = client->Write("Hello, world!\r\n");
                if (bytes_sent > 0) {
                    std::printf("Sent greeting to client %d (%d bytes)\n", connection_count, bytes_sent);
                } else {
                    std::fprintf(stderr, "Error: Failed to send greeting to client %d\n", connection_count);
                }

                // Make client non-blocking for Read()
                client->SetBlocking(false);

                // Add to our client list
                clients.push_back(client);
            }
        }

        // Poll all connected clients (non-blocking reads)
        for (auto it = clients.begin(); it != clients.end(); ) {
            net::CSocket* c = *it;
            if (c->connected) {
                char buffer[256];
                int bytes_read = c->Read(buffer, sizeof(buffer) - 1);
                
                if (bytes_read > 0) {
                    buffer[bytes_read] = '\0';
                    std::printf("Client data: %s\n", buffer);
                    ++it;
                } else if (bytes_read == 0) {
                    // Connection closed by client
                    std::printf("Client disconnected\n");
                    c->Close();
                    delete c;
                    it = clients.erase(it);
                } else {
                    // No data available (non-blocking), or error
                    // With non-blocking, this is normal - just skip
                    ++it;
                }
            } else {
                // Client disconnected
                c->Close();
                delete c;
                it = clients.erase(it);
            }
        }

        // Small sleep to prevent busy-waiting and allow signal processing
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // Clean up remaining clients
    for (auto client : clients) {
        if (client) {
            client->Close();
            delete client;
        }
    }

    // Close server socket
    server.Close();
    
    // Wait for acceptor thread to finish
    if (acceptor.joinable()) {
        acceptor.join();
    }

    std::cout << "Server shutdown complete.\n";
    return EXIT_SUCCESS;
}