/******************************************************************
 * http-client.cpp - HTTP/HTTPS client using libaeon
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#include <iostream>
#include <cstdio>
#include <cstring>
#include <string>
#include <net.h>

#ifdef ENABLE_SSL
#include <ssl_client_socket.h>
#endif

#define HTML_HEADER_BREAK "\r\n\r\n"
#define MAX_REDIRECTS 5

struct url_parts {
    bool is_https;
    std::string domain;
    std::string path;
    int port;
};

url_parts parse_url(const std::string& url) {
    url_parts parts{false, "", "/", 80};
    std::string working_url = url;

    // Check for https://
    if (working_url.find("https://") == 0) {
        parts.is_https = true;
        parts.port = 443;
        working_url = working_url.substr(8);
    }
    // Check for http://
    else if (working_url.find("http://") == 0) {
        parts.is_https = false;
        parts.port = 80;
        working_url = working_url.substr(7);
    }

    // Split domain and path
    std::size_t slash_pos = working_url.find("/");
    if (slash_pos != std::string::npos) {
        parts.path = working_url.substr(slash_pos);
        parts.domain = working_url.substr(0, slash_pos);
    } else {
        parts.domain = working_url;
    }

    return parts;
}

std::string extract_redirect_url(const std::string& headers) {
    // Look for Location header
    std::string location_key = "Location: ";
    std::size_t loc_pos = headers.find(location_key);
    
    if (loc_pos != std::string::npos) {
        std::size_t start = loc_pos + location_key.length();
        std::size_t end = headers.find("\r\n", start);
        if (end == std::string::npos) {
            end = headers.find("\n", start);
        }
        if (end != std::string::npos) {
            return headers.substr(start, end - start);
        }
    }
    return "";
}

bool send_request(net::socket* socket, const std::string& domain, const std::string& path) {
    std::string http_request = "GET " + path + " HTTP/1.1\r\n";
    http_request += "Host: " + domain + "\r\n";
    http_request += "Connection: close\r\n";
    http_request += "\r\n";

    int bytes_sent = socket->write(http_request.c_str());
    return bytes_sent > 0;
}

std::string read_response(net::socket* socket) {
    std::string response;
    char buffer[512];
    int bytes_read = 0;

    do {
        bytes_read = socket->read(buffer, sizeof(buffer) - 1);
        if (bytes_read > 0) {
            response.append(buffer, bytes_read);
        }
    } while (bytes_read > 0);

    return response;
}

int main(int argc, char** argv) {
    std::cout << "Checking arguments..." << std::endl;
    
    if (argc < 2) {
        std::fprintf(stderr, "Usage: %s URL [--verify] [--cacert <path>]\n", argv[0]);
        std::fprintf(stderr, "  URL: The HTTP or HTTPS URL to fetch\n");
        std::fprintf(stderr, "  --verify: Enable peer certificate verification (default: off)\n");
        std::fprintf(stderr, "  --cacert <path>: Path to CA certificate bundle file\n");
        return EXIT_FAILURE;
    }

    std::string current_url = argv[1];
    bool verify_peer = false;
    std::string ca_cert_file;
    
    // Parse optional arguments
    for (int i = 2; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--verify") {
            verify_peer = true;
        } else if (arg == "--cacert" && i + 1 < argc) {
            ca_cert_file = argv[++i];
        }
    }

    int redirect_count = 0;

    while (redirect_count < MAX_REDIRECTS) {
        url_parts parts = parse_url(current_url);

        std::cout << "URL: " << current_url << std::endl;
        std::cout << "Domain: " << parts.domain << ", Path: " << parts.path;
        std::cout << ", Port: " << parts.port;
        std::cout << ", Protocol: " << (parts.is_https ? "HTTPS" : "HTTP") << std::endl;

        if (parts.domain.empty()) {
            std::fprintf(stderr, "Invalid URL\n");
            return EXIT_FAILURE;
        }

        // Create appropriate socket
        net::socket* socket = nullptr;
        bool connected = false;

        if (parts.is_https) {
#ifdef ENABLE_SSL
            auto* ssl_sock = new net::ssl_client_socket();
            
            // Set verification mode
            if (verify_peer) {
                ssl_sock->set_peer_verification(net::ssl_verify_mode::strict);
                if (!ca_cert_file.empty()) {
                    int ca_result = ssl_sock->set_ca_file(ca_cert_file.c_str());
                    if (ca_result != 0) {
                        std::fprintf(stderr, "Failed to load CA certificate file: %s\n", ca_cert_file.c_str());
                        delete ssl_sock;
                        return EXIT_FAILURE;
                    }
                }
            } else {
                ssl_sock->set_peer_verification(net::ssl_verify_mode::off);
            }
            
            if (!ssl_sock->connect(parts.domain.c_str(), parts.port)) {
                std::fprintf(stderr, "SSL connection failed: %d\n", ssl_sock->get_error());
                return EXIT_FAILURE;
            }
            socket = ssl_sock;
            connected = ssl_sock->connected;
            if (!connected) {
                std::fprintf(stderr, "SSL connection failed: %d\n", ssl_sock->get_error());
            }
#else
            std::fprintf(stderr, "HTTPS requested but SSL support not enabled\n");
            return EXIT_FAILURE;
#endif
        } else {
            auto* client_sock = new net::client_socket(parts.domain.c_str(), parts.port);
            socket = client_sock;
            connected = client_sock->connected;
            if (!connected) {
                std::fprintf(stderr, "Connection failed: %d\n", client_sock->get_error());
            }
        }

        if (!connected) {
            delete socket;
            return EXIT_FAILURE;
        }

        // Send request
        std::cout << "Sending request..." << std::endl;
        if (!send_request(socket, parts.domain, parts.path)) {
            std::fprintf(stderr, "Failed to send request\n");
            delete socket;
            return EXIT_FAILURE;
        }

        // Read response
        std::cout << "Reading response..." << std::endl;
        std::string response = read_response(socket);
        socket->close();

        // Parse response
        std::size_t header_end = response.find(HTML_HEADER_BREAK);
        std::string headers = (header_end != std::string::npos) ? response.substr(0, header_end) : response;
        std::string body = (header_end != std::string::npos) ? response.substr(header_end + 4) : "";

        // Check for redirect
        if (headers.find("301") != std::string::npos || 
            headers.find("302") != std::string::npos ||
            headers.find("303") != std::string::npos ||
            headers.find("307") != std::string::npos ||
            headers.find("308") != std::string::npos) {
            
            std::string redirect_url = extract_redirect_url(headers);
            if (!redirect_url.empty()) {
                std::cout << "Redirect to: " << redirect_url << std::endl;
                current_url = redirect_url;
                redirect_count++;
                delete socket;
                continue;
            }
        }

        delete socket;

        // Output response body
        std::cout << "\n=== Response Body ===" << std::endl;
        std::cout << body << std::endl;
        std::cout << "=== End of Response ===" << std::endl;

        return EXIT_SUCCESS;
    }

    std::fprintf(stderr, "Too many redirects\n");
    return EXIT_FAILURE;
}