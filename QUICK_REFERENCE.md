# Quick API Reference: I/O Timeouts and TCP_NODELAY

## Setting Timeouts

### On Any Socket (CSocket, CClientSocket, CServerSocket)

```cpp
socket.SetReadTimeout(5000);      // 5 second read timeout
socket.SetWriteTimeout(5000);     // 5 second write timeout
socket.SetTCPNodelay(true);       // Disable Nagle's algorithm
```

### On Server Socket

```cpp
server.SetAcceptTimeout(100);     // 100ms accept timeout
```

### On Client Socket

```cpp
client.SetConnectTimeout(10000);  // 10 second connect timeout (future)
```

## Typical Server Configuration

```cpp
net::CServerSocket server;
server.Listen(2300);
server.SetAcceptTimeout(100);     // Wake up every 100ms for shutdown check

while (!shutdown) {
    net::CSocket* client = server.Accept();
    if (client && client->connected) {
        // Low-latency configuration
        client->SetTCPNodelay(true);
        client->SetReadTimeout(30000);   // 30 sec idle timeout
        client->SetWriteTimeout(5000);   // 5 sec send timeout
        
        // Your client handling code...
    }
}
```

## Typical Client Configuration

```cpp
net::CClientSocket client;
client.SetConnectTimeout(10000);  // 10 second connect timeout
client.SetReadTimeout(5000);      // 5 second read timeout
client.SetWriteTimeout(5000);     // 5 second write timeout
client.SetTCPNodelay(true);       // Low latency

if (client.Connect("example.com", 80)) {
    // Connected successfully
}
```

## Reading with Timeout

```cpp
int bytes = socket.Read(buffer, sizeof(buffer));

if (bytes > 0) {
    // Data received successfully
} else if (bytes == 0) {
    // Timeout occurred (not ready yet)
} else {
    // Error occurred
}
```

## Writing with Timeout

```cpp
int bytes = socket.Write(message, strlen(message));

if (bytes > 0) {
    // Data sent successfully
} else if (bytes == 0) {
    // Timeout occurred (socket not ready)
} else {
    // Error occurred
}
```

## Polling Loop Example

```cpp
socket.SetBlocking(false);
socket.SetReadTimeout(100);  // 100ms timeout per read

while (connected) {
    int bytes = socket.Read(buffer, sizeof(buffer));
    
    if (bytes > 0) {
        ProcessData(buffer, bytes);
    } else if (bytes == 0) {
        // Timeout or no data (with non-blocking)
        // This is normal, just continue
        continue;
    } else {
        // Error occurred
        connected = false;
    }
}
```

## TCP_NODELAY Explanation

**Disabled (default):**
```cpp
socket.SetTCPNodelay(false);  // Or just use default
// TCP may buffer small packets waiting for more data
// Lower bandwidth usage, higher latency (~40ms+)
// Good for: FTP, HTTP, file transfers
```

**Enabled:**
```cpp
socket.SetTCPNodelay(true);
// Each send() becomes its own packet
// Lower latency (~1-5ms), higher bandwidth usage
// Good for: Games, chat, real-time applications
```

## Timeout Values (in milliseconds)

- **0** = No timeout (blocking mode) - default
- **1-10** = Very responsive, high CPU usage
- **50-100** = Good for game servers
- **500-1000** = Good for general networking
- **5000+** = Good for idle detection

## Platform Notes

Both Windows and Linux/macOS use the same API:
- All methods return 0 on success
- All timeouts are in milliseconds
- All methods are non-blocking when setting values

## Error Handling

```cpp
// Generic pattern
int result = socket.SetReadTimeout(5000);
if (result != 0) {
    printf("Error: %d\n", socket.GetError());
}

// Actually, timeout setters just store the value
// Errors only occur during actual read/write
int bytes = socket.Read(buffer, sizeof(buffer));
if (bytes < 0) {
    printf("Read error: %d\n", socket.GetError());
}
```

## Common Patterns

### High-Performance Game Server
```cpp
client->SetTCPNodelay(true);
client->SetReadTimeout(16);     // ~60 FPS
client->SetWriteTimeout(16);
client->SetBlocking(false);
```

### Responsive Chat Application
```cpp
client->SetTCPNodelay(true);
client->SetReadTimeout(50);
client->SetWriteTimeout(100);
client->SetBlocking(false);
```

### Robust HTTP Server
```cpp
client->SetReadTimeout(30000);  // 30 sec for headers
client->SetWriteTimeout(10000); // 10 sec for response
client->SetTCPNodelay(false);   // Allow Nagle's buffering
```

### Reliable Data Transfer
```cpp
client->SetReadTimeout(5000);
client->SetWriteTimeout(5000);
client->SetTCPNodelay(false);   // Better for bulk data
client->SetBlocking(true);
```