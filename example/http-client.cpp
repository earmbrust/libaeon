/******************************************************************
 * http-client.cpp - A simple HTTP client using libaeon
 * Copyright (c) 2006-2025 Elden Armbrust
 ******************************************************************/

#include <iostream>
#include <cstdio>
#include <cstring>
#include <string>
#include <libaeon.h>

#define HTML_HEADER_BREAK "\r\n\r\n"  // Separator between HTTP header and content (RFC 7230)

int main(int argc, char** argv) {
    std::cout << "Checking arguments..." << std::endl;
    
    if (argc != 2) {
        std::fprintf(stderr, "Usage: %s URL\n", argv[0]);
        return EXIT_FAILURE;
    }

    std::cout << "Parsing URL..." << std::endl;
    
    // Parse the command-line URL
    std::string url_arg = argv[1];
    std::string domain;
    std::string page = "/";

    // Remove "http://" prefix if present
    if (url_arg.find("http://") != std::string::npos) {
        url_arg = url_arg.substr(7);
    }

    // Split domain and path
    std::size_t slash_pos = url_arg.find("/");
    if (slash_pos != std::string::npos) {
        page = url_arg.substr(slash_pos);
        domain = url_arg.substr(0, slash_pos);
    } else {
        domain = url_arg;
    }

    std::cout << "Domain: " << domain << ", Path: " << page << std::endl;

    if (page.empty() || domain.empty()) {
        std::fprintf(stderr, "Invalid URL\n");
        return EXIT_FAILURE;
    }

    // Create and connect socket
    std::cout << "Creating socket and connecting..." << std::endl;
    net::CClientSocket socket(domain.c_str(), 80);

    if (!socket.connected) {
        std::fprintf(stderr, "Connection failed!\n");
        return EXIT_FAILURE;
    }

    std::cout << "Connected. Sending HTTP request..." << std::endl;

    // Build HTTP request
    std::string http_request = "GET " + page + " HTTP/1.0\r\n";
    http_request += "Host: " + domain + "\r\n";
    http_request += "Connection: close\r\n";
    http_request += "\r\n";

    // Send request
    int bytes_sent = socket.Write(http_request.c_str());
    if (bytes_sent <= 0) {
        std::fprintf(stderr, "Failed to send request\n");
        return EXIT_FAILURE;
    }

    std::cout << "Request sent (" << bytes_sent << " bytes). Reading response..." << std::endl;

    // Read and display response
    char buffer[256];
    int bytes_read = 0;
    bool headers_done = false;

    do {
        bytes_read = socket.Read(buffer, sizeof(buffer) - 1);
        
        if (bytes_read <= 0) {
            break;
        }

        buffer[bytes_read] = '\0';
        std::string response_chunk = buffer;

        // Find end of headers
        if (!headers_done) {
            std::size_t header_end = response_chunk.find(HTML_HEADER_BREAK);
            if (header_end != std::string::npos) {
                headers_done = true;
                // Print body content after headers
                std::size_t body_start = header_end + std::strlen(HTML_HEADER_BREAK);
                if (body_start < response_chunk.size()) {
                    std::cout << response_chunk.substr(body_start);
                }
            }
        } else {
            // Headers already processed, just print the content
            std::cout << response_chunk;
        }
    } while (bytes_read > 0);

    socket.Close();
    std::cout << "\nRequest complete." << std::endl;
    
    return EXIT_SUCCESS;
}