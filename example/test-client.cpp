/******************************************************************
 * test-client.cpp - A simple TCP test client using libaeon
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/

#ifdef _MSC_VER
#pragma warning(disable:4251)
#endif

#include <net.h>
#include <iostream>

int main() {
    net::client_socket client;
    client.set_connect_timeout(2000);  // 2 second timeout
    
    // Test with direct IPv4 address to bypass IPv6 resolution
    if (client.connect("127.0.0.1", 2300)) {
        std::cout << "Connected!" << std::endl;
        
        char buffer[256];
        int bytes_read = client.read(buffer, sizeof(buffer) - 1);
        buffer[bytes_read] = '\0';
        
        std::cout << "Server says: " << buffer << std::endl;
        
        client.write("Hello from client!");
    } else {
        std::cout << "Connection failed" << std::endl;
    }
    
    return 0;
}