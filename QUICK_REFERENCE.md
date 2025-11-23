# Quick API Reference: Socket Configuration

## Setting Timeouts

### Read Timeout
```cpp
socket.SetReadTimeout(5000);  // 5 second timeout for Read() calls
```
- Stores the timeout value used by Read() operations
- Uses select() internally to wait before recv()
- Returns 0 on success, -1 on error
- Default: 0 (no timeout, blocking mode)

### Write Timeout
```cpp
socket.SetWriteTimeout(5000);  // Currently stored but not actively used in Write()
```
- **Note**: This value is stored but not currently applied during Write() operations
- Intended for future use
- Returns 0 on success, -1 on error
- Default: 0

### Connect Timeout (TCP Client Only)
```cpp
client.SetConnectTimeout(10000);  // Currently stored but not actively used in Connect()
```
- **Note**: This value is stored but not currently applied during Connect() operations
- Intended for future use
- Returns 0 on success, -1 on error
- Default: 0

## Setting Socket Modes

### Blocking Mode
```cpp
socket.SetBlocking(true);   // Blocking mode (default)
socket.SetBlocking(false);  // Non-blocking mode
```
- Actually applies the mode immediately to the socket
- Returns: 1 if blocking, 0 if non-blocking, -1 on error
- On Windows: Uses ioctlsocket(FIONBIO)
- On Linux/macOS: Uses fcntl(F_SETFL)

### TCP_NODELAY (Disable Nagle's Algorithm)
```cpp
socket.SetTCPNodelay(true);   // Disable Nagle (low-latency mode)
socket.SetTCPNodelay(false);  // Enable Nagle (default, buffering enabled)
```
- Actually applies immediately via setsockopt(TCP_NODELAY)
- Returns 0 on success, -1 on error
- Only applies to TCP sockets (CSocket, CClientSocket, CServerSocket, CEventSocket)
- UDP sockets don't use this

## Reading with Timeout

```cpp
socket.SetReadTimeout(5000);  // 5 second timeout
int bytes = socket.Read(buffer, sizeof(buffer));

if (bytes > 0) {
    // Data received successfully
} else if (bytes == 0) {
    // Timeout occurred - no data available
} else {
    // Error occurred (bytes == -1)
    // Check socket.GetError() for error code
}
```

## Typical Server Configuration

```cpp
net::CServerSocket server;
server.Listen(8080);

while (!shutdown) {
    net::CEventSocket* client = server.Accept();
    if (client && client->connected) {
        // Apply low-latency settings
        client->SetTCPNodelay(true);
        client->SetReadTimeout(30000);   // 30 sec read timeout
        
        // Non-blocking mode for event-driven handling
        client->SetBlocking(false);
        
        // Your client handling...
        delete client;
    }
}
```

## Typical Client Configuration

```cpp
net::CClientSocket client("example.com", 80);

if (client.connected) {
    client.SetTCPNodelay(true);        // Low-latency mode
    client.SetReadTimeout(5000);       // 5 second read timeout
    client.SetBlocking(true);          // Blocking for simple request/response
    
    client.Write("GET / HTTP/1.1\r\n");
    std::string response = client.Read(1024);
}
```

## Return Values

**Timeout Setters** (SetReadTimeout, SetWriteTimeout, SetConnectTimeout):
- Returns: **0** (always succeeds, just stores the value)
- Error: Not checked - just stores the value

**SetBlocking:**
- Returns: **1** if blocking mode is now active
- Returns: **0** if non-blocking mode is now active  
- Returns: **-1** on error (check GetError())

**SetTCPNodelay:**
- Returns: **0** on success
- Returns: **-1** on error (check GetError())

**Read/Write with timeouts:**
- Returns: **> 0** = bytes read/sent successfully
- Returns: **0** = timeout occurred (no data available)
- Returns: **-1** = error occurred (check GetError())

## How Timeouts Work

### Read Timeout Implementation
```cpp
// In CSocket::Read():
if (this->read_timeout_ms > 0) {
    int wait_result = WaitForReadable(sockfd, read_timeout_ms);
    if (wait_result == 0) {
        return 0;  // Timeout
    } else if (wait_result < 0) {
        return -1; // Error
    }
}
// Then recv() is called
```

Uses select() to wait for the socket to be readable with the specified timeout.

### Write Timeout
Currently stored but **not applied** during Write() operations. Future implementation.

### Connect Timeout
Currently stored but **not applied** during Connect() operations. Future implementation.

## Platform Support

All socket configuration methods work on:
- Windows (MSVC, MinGW)
- Linux (GCC, Clang)
- macOS (Clang)

Platform-specific implementation details are abstracted away.

## Non-Blocking Socket Example

```cpp
net::CEventSocket* socket = new net::CEventSocket();
socket->SetBlocking(false);
socket->SetReadTimeout(100);  // Quick check, don't wait long

while (socket->connected) {
    int bytes = socket->Read(buffer, sizeof(buffer));
    
    if (bytes > 0) {
        // Process received data
        ProcessData(buffer, bytes);
    } else if (bytes == 0) {
        // Timeout - no data this iteration, continue
        continue;
    } else {
        // Error
        socket->connected = false;
    }
}
```

## Common Patterns

### Simple Blocking Client
```cpp
client.SetBlocking(true);          // Default
client.SetReadTimeout(5000);       // 5 sec per read
client.SetTCPNodelay(false);       // Default, allow buffering
```

### Low-Latency Server (Game-like)
```cpp
client->SetBlocking(false);
client->SetReadTimeout(16);        // ~60 FPS tick rate
client->SetTCPNodelay(true);       // Immediate sends
```

### Responsive Interactive Session
```cpp
client->SetBlocking(false);
client->SetReadTimeout(50);
client->SetTCPNodelay(true);
```

### Bulk Data Transfer
```cpp
socket.SetBlocking(true);
socket.SetReadTimeout(30000);      // Long timeout for slow transfers
socket.SetTCPNodelay(false);       // Allow Nagle's buffering
```

## Error Checking

```cpp
int result = socket.SetBlocking(false);
if (result == -1) {
    printf("Error: %d\n", socket.GetError());
}

// For timeouts, check return value from Read/Write
int bytes = socket.Read(buffer, 256);
if (bytes == -1) {
    printf("Read error: %d\n", socket.GetError());
}
```

## Known Limitations

- **SetWriteTimeout**: Stored but not currently used during Write()
- **SetConnectTimeout**: Stored but not currently used during Connect()
- **Read timeout only**: Write operations do not currently respect write_timeout_ms
- **Select-based**: Timeout implementation uses select(), which has platform-specific fd_set limits on Windows