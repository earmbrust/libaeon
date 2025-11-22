/******************************************************************
 * simple-server-with-timeouts.cpp - Server with I/O timeouts and TCP_NODELAY
 * Demonstrates the new timeout and TCP_NODELAY features
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

static std::atomic<bool> shutdown_requested(false);

void signal_handler(int sig) {
    if (sig == SIGINT) {
        std::cout << "\n*** Shutdown requested...\n";
        shutdown_requested = true;
    }
}

int main(void) {
    std::cout << "Starting server with I/O timeouts and TCP_NODELAY\n";
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

    // Accept timeout so thread wakes up frequently to check shutdown
    server.SetAcceptTimeout(100);

    std::cout << "Server listening...\n";

    std::vector<net::CSocket*> clients;
    std::vector<net::CSocket*> pending_clients;
    std::mutex pending_lock;
    int connection_count = 0;

    // Accept thread with responsive shutdown
    auto accept_thread_func = [&]() {
        std::cout << "[Accept Thread] Started\n";
        
        while (!shutdown_requested) {
            net::CSocket* client = server.Accept();
            
            if (client && client->connected) {
                {
                    std::lock_guard<std::mutex> lock(pending_lock);
                    pending_clients.push_back(client);
                }
                std::cout << "[Accept Thread] New connection queued\n";
            } else if (client) {
                delete client;
            }
            
            if (shutdown_requested) break;
        }
        
        std::cout << "[Accept Thread] Exiting\n";
    };

    std::thread acceptor(accept_thread_func);

    // Main server loop
    while (!shutdown_requested) {
        // Process new connections
        {
            std::lock_guard<std::mutex> lock(pending_lock);
            while (!pending_clients.empty()) {
                net::CSocket* client = pending_clients.back();
                pending_clients.pop_back();
                ++connection_count;
                std::printf("Client %d connected\n", connection_count);

                // Configure socket for low-latency communication
                // Disable Nagle's algorithm - send data immediately
                client->SetTCPNodelay(true);
                
                // Set read timeout to 30 seconds (idle timeout)
                client->SetReadTimeout(30000);
                
                // Set write timeout to 5 seconds
                client->SetWriteTimeout(5000);

                // Send greeting
                int bytes_sent = client->Write("Hello, world!\r\n");
                if (bytes_sent > 0) {
                    std::printf("Sent greeting to client %d (%d bytes)\n", 
                               connection_count, bytes_sent);
                } else if (bytes_sent == 0) {
                    std::fprintf(stderr, "Error: Write timeout sending greeting to client %d\n", 
                                connection_count);
                } else {
                    std::fprintf(stderr, "Error: Failed to send greeting to client %d\n", 
                                connection_count);
                }

                // Make client non-blocking for Read() - we'll use timeouts
                client->SetBlocking(false);

                clients.push_back(client);
            }
        }

        // Poll all connected clients with read timeout
        for (auto it = clients.begin(); it != clients.end(); ) {
            net::CSocket* c = *it;
            if (c->connected) {
                char buffer[256];
                int bytes_read = c->Read(buffer, sizeof(buffer) - 1);
                
                if (bytes_read > 0) {
                    buffer[bytes_read] = '\0';
                    std::printf("Client data: %s", buffer);
                    ++it;
                } else if (bytes_read == 0) {
                    // Either timeout (with non-blocking read) or connection closed
                    // Since we have non-blocking reads, 0 could mean either
                    ++it;
                } else {
                    // Error or connection closed
                    std::printf("Client %d disconnected\n", 
                               (int)(it - clients.begin()) + 1);
                    c->Close();
                    delete c;
                    it = clients.erase(it);
                }
            } else {
                c->Close();
                delete c;
                it = clients.erase(it);
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // Cleanup
    for (auto client : clients) {
        if (client) {
            client->Close();
            delete client;
        }
    }

    server.Close();
    
    if (acceptor.joinable()) {
        acceptor.join();
    }

    std::cout << "Server shutdown complete.\n";
    return EXIT_SUCCESS;
}