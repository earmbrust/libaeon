/******************************************************************
 * simple-server.cpp - A simple event-driven server using libaeon
 * Copyright (c) 2006-2025 Elden Armbrust
 ******************************************************************/

#include <libaeon.h>
#include <iostream>
#include <cstdio>
#include <vector>

#define SERVER_PORT 2300

/**
 * Custom event socket class that handles client connections
 */
class ClientSocket : public net::CEventSocket {
public:
    bool OnRead(const char* buffer, int size);
};

bool ClientSocket::OnRead(const char* buffer, int size) {
    if (size > 0) {
        std::printf("Client said: %s\n", buffer);
    }
    return true;
}

int main(void) {
    std::cout << "Starting simple event-driven server on port " << SERVER_PORT << std::endl;

    // Create server socket
    net::CServerSocket server;

    // Listen on the specified port
    if (!server.Listen(SERVER_PORT)) {
        std::fprintf(stderr, "Error: Failed to listen on port %d\n", SERVER_PORT);
        return EXIT_FAILURE;
    }

    std::cout << "Server listening... waiting for connections\n";

    // Create socket set for managing multiple clients
    std::vector<net::CSocket*> clients;
    int connection_count = 0;

    // Main server loop
    while (true) {
        // Try to accept a new connection (non-blocking)
        net::CSocket* client = server.Accept();
        
        if (client && client->connected) {
            ++connection_count;
            std::printf("Client %d connected\n", connection_count);

            // Send greeting message to client
            int bytes_sent = client->Write("Hello, world!\r\n");
            if (bytes_sent > 0) {
                std::printf("Sent greeting to client %d (%d bytes)\n", connection_count, bytes_sent);
            } else {
                std::fprintf(stderr, "Error: Failed to send greeting to client %d\n", connection_count);
            }

            // Add to our client list
            clients.push_back(client);
        } else if (client) {
            delete client;
        }

        // Poll all connected clients
        for (auto it = clients.begin(); it != clients.end(); ) {
            net::CSocket* c = *it;
            if (c->connected) {
                char buffer[256];
                int bytes_read = c->Read(buffer, sizeof(buffer) - 1);
                if (bytes_read > 0) {
                    buffer[bytes_read] = '\0';
                    std::printf("Client data: %s\n", buffer);
                }
                ++it;
            } else {
                // Client disconnected
                c->Close();
                delete c;
                it = clients.erase(it);
            }
        }
    }

    return EXIT_SUCCESS;
}