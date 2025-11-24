/******************************************************************
 * test-client.cpp - A simple TCP test client using libaeon
 * Copyright 2006-2025 (c) Elden Armbrust
 * This software is licensed under the BSD software license.
 *********************************************************************/
#include <libaeon.h>
#include <iostream>

int main() {
    net::CClientSocket client("localhost", 2300);
    
    if (client.connected) {
        std::cout << "Connected!" << std::endl;
        
        char buffer[256];
        int bytes_read = client.Read(buffer, sizeof(buffer) - 1);
        buffer[bytes_read] = '\0';
        
        std::cout << "Server says: " << buffer << std::endl;
        
        client.Write("Hello from client!");
    } else {
        std::cout << "Connection failed" << std::endl;
    }
    
    return 0;
}