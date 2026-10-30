# Quick API Reference: Socket Configuration

Include `<aeon.hpp>` to use the libAeon 2.0 API. Public types are in the
`aeon` namespace, and methods use lowercase `snake_case`. Timeouts are in
milliseconds. A timeout of `0` disables the corresponding readiness wait; it
does not change a socket's blocking mode.

## Setting Timeouts

### Read Timeout
```cpp
client.set_read_timeout(5000);  // Wait up to 5 seconds for readable data
```
- `socket::read(char*, int)`, `read()` and `read(int)` use `select()` to wait for readability before receiving data. UDP reads also use the configured timeout.
- A positive timeout returns `0` from the buffer overload when no data becomes readable. For TCP, `0` can also mean an orderly disconnect.
- `read_line()` uses its own byte-by-byte receive loop and does not apply the configured read timeout.
- Returns `0` on success and `-1` for a negative timeout. Default: `0`.

### Write Timeout
```cpp
client.set_write_timeout(5000);  // Wait for writability before an explicit-size write
```
- The base TCP `socket::write(char*, int)` and `write(const char*, int)` overloads check writability with `select()` before `send()`.
- These overloads return `0` when the readiness wait times out; a successful wait can still result in a partial send.
- The base C-string and `std::string` write overloads send directly without consulting this timeout. Event and TLS socket transports have their own I/O paths.
- This is a readiness wait, not a deadline for transferring every byte.
- Returns `0` on success and `-1` for a negative timeout. Default: `0`.

### Connect Timeout (TCP Client Only)
```cpp
aeon::client_socket client;
client.set_connect_timeout(10000);  // 10 seconds for TCP connect readiness
```
- With a positive timeout, TCP connect temporarily uses nonblocking mode, waits for writability and checks `SO_ERROR` before reporting success.
- The original blocking mode is restored after the attempt. A failed connection closes the attempted descriptor.
- With the default `0`, the native connect call is used without a library-imposed timeout.
- Returns `0` on success and `-1` for a negative timeout.

### Accept Timeout (TCP Servers)
```cpp
aeon::server_event_socket server;
server.set_accept_timeout(2000);  // Bound the wait for an incoming connection
```
- Available on `server_socket` and `server_event_socket`.
- Positive values wait for listener readability before accepting; an expired wait returns `nullptr` without treating the timeout as a socket error.
- `accept(false)` still configures the accepted peer as nonblocking. The timeout controls the wait for a pending connection, not the peer's mode.
- Returns `0` on success and `-1` for values outside `0` through `60000`. Default: `0`.

## Setting Socket Modes

### Blocking Mode
```cpp
client.set_blocking(true);   // Blocking mode (default)
client.set_blocking(false);  // Nonblocking mode
```
- Applies the mode to the native socket immediately.
- Returns `0` on success, `-1` on error; it does not return the selected mode.
- On Windows, uses `ioctlsocket(FIONBIO)`; on POSIX, uses `fcntl(F_SETFL)`.
- A timeout does not by itself make a socket nonblocking.

### TCP_NODELAY (Disable Nagle's Algorithm)
```cpp
client.set_tcp_nodelay(true);   // Disable Nagle's algorithm
client.set_tcp_nodelay(false);  // Enable Nagle's algorithm
```
- Applies `TCP_NODELAY` with `setsockopt()` on TCP sockets.
- Returns `0` on success, `-1` on error. This option is not for UDP sockets.

## Reading with Timeout

```cpp
char buffer[1024];
client.set_read_timeout(5000);
const int bytes = client.read(buffer, sizeof(buffer));

if (bytes > 0) {
    // Process exactly bytes bytes; the buffer is not NUL-terminated.
} else if (bytes == 0) {
    if (!client.connected) {
        // TCP peer closed its sending side.
    } else {
        // No bytes were returned before the configured timeout.
    }
} else {
    // Native receive error; inspect client.get_error().
}
```

The buffer overload preserves binary data, including embedded NULs. `read(int)`
returns a binary-safe `std::string`, but an empty result can mean timeout, EOF
or error. The no-argument `read()` uses the legacy internal 256-byte buffer
and reads at most 255 bytes at a time.

## Typical Server Configuration

```cpp
aeon::server_socket server;
if (server.listen(8080)) {
    server.set_accept_timeout(2000);
    if (aeon::socket* client = server.accept(true)) {
        client->set_tcp_nodelay(true);
        client->set_read_timeout(30000);
        // Handle the client. The server owns the accepted socket.
        server.remove_peer(client);  // Retire it when finished.
    }
}
```

Accepted peers are owned by the server. Do not `delete` the returned pointer.
`remove_peer()` destroys the peer and invalidates borrowed pointers. Closing
the server closes its owned peers; the peer objects remain owned until removal
or server destruction.

## Typical Client Configuration

```cpp
aeon::client_socket client;
client.set_connect_timeout(10000);
if (client.connect("example.com", 80)) {
    client.set_tcp_nodelay(true);
    client.set_read_timeout(5000);
    client.set_write_timeout(5000);

    const char request[] = "GET / HTTP/1.1\r\nHost: example.com\r\nConnection: close\r\n\r\n";
    const int sent = client.write(request, sizeof(request) - 1);
    if (sent > 0) {
        const std::string response = client.read(1024);
        // Handle response; both send and receive can be partial.
    }
}
```

TCP is a byte stream. Applications must handle partial writes, partial reads
and their own message framing; one `read()` is not guaranteed to return a full
response.

## Return Values

**Timeout setters** (`set_read_timeout`, `set_write_timeout`, `set_connect_timeout`):
- `0` on success; `-1` for a negative value. These setters store a configuration value and do not require an open descriptor.

**`set_accept_timeout`:**
- `0` on success; `-1` outside the allowed range of 0-60000 ms.

**`set_blocking` and `set_tcp_nodelay`:**
- `0` on success; `-1` if the socket operation fails.

**Buffer `read` and explicit-size base TCP `write`:**
- Positive: bytes received or sent (not necessarily the requested amount).
- Zero: read timeout or TCP EOF; on explicit-size write, readiness timeout.
- `-1`: invalid arguments, invalid descriptor or native operation failure.

**`read_until(char*, int)`:**
- Returns the accumulated byte count, including a partial count if EOF, timeout or error interrupts the operation; `-1` for invalid arguments or an invalid descriptor.

## How Timeouts Work

### Read Timeout Implementation

For positive timeouts, a socket read checks readability with `select()` and
then calls `recv()`. If the readiness wait expires, the buffer overload returns
`0` and stores `n = 0`. TCP `recv()` also returns `0` at EOF, in which case
`connected` becomes `false`.

### Write Timeout

The base explicit-size TCP buffer overloads check writability with `select()`
before calling `send()`. The C-string and `std::string` overloads do not use
this timeout. A writable descriptor does not guarantee that `send()` transmits
the entire buffer or that subsequent native operations cannot block.

### Connect Timeout

TCP client connections with a positive timeout use a temporary nonblocking
connection attempt, wait for writability, inspect `SO_ERROR`, and restore the
previous blocking mode. A timeout fails the connection; it does not return a
connected socket.

## Platform Support

The socket configuration API is available on Windows, Linux and macOS.
Readiness behavior can differ by platform. Errors reported through `get_error()`
use libAeon's portable error constants.

## Non-Blocking Socket Example

```cpp
aeon::client_socket client;
if (client.connect("127.0.0.1", 2300) && client.set_blocking(false) == 0) {
    char buffer[256];
    const int bytes = client.read(buffer, sizeof(buffer));
    if (bytes > 0) {
        // Process bytes received.
    } else if (bytes < 0) {
        // No data may produce a platform-specific would-block error.
        // Use readiness polling rather than spinning on read().
    }
}
```

Event-driven applications can use `event_socket_set::poll()` instead of
manually polling each socket. Event sockets require event support enabled at
build time.

## Common Patterns

### Simple Blocking Client
```cpp
client.set_blocking(true);
client.set_read_timeout(5000);
client.set_tcp_nodelay(false);
```

### Low-Latency Server (Game-like)
```cpp
client->set_blocking(false);
client->set_tcp_nodelay(true);
// Integrate the peer with your event poll loop.
```

### Responsive Interactive Session
```cpp
client->set_read_timeout(50);
client->set_tcp_nodelay(true);
```

### Bulk Data Transfer
```cpp
client.set_blocking(true);
client.set_read_timeout(30000);
client.set_tcp_nodelay(false);
```

## Portable Error Codes

`get_error()` returns libAeon error constants on socket failures; applications
never need to compare `errno` or Winsock error numbers. Native socket errors are
translated internally. For example, POSIX `ECONNRESET` and Winsock
`WSAECONNRESET` both become `aeon::err_connection_reset`.

```cpp
if (client.write("hello", 5) < 0) {
    if (client.get_error() == aeon::err_connection_reset) {
        // The peer reset the connection.
    }
}
```

The core errors use `-0x01xxxx`, TLS errors use `-0x02xxxx`, and
`aeon::err_system` identifies an unmapped native failure. `-1` remains the
standard I/O failure return value; `get_error()` provides the reason.
An expired positive read timeout returns `0` and sets `aeon::err_timeout`;
TCP EOF and a zero-byte UDP datagram are not errors.

## Error Checking

```cpp
if (client.set_blocking(false) != 0) {
    // client.get_error() reports the configuration error.
}

char buffer[256];
const int bytes = client.read(buffer, sizeof(buffer));
if (bytes < 0) {
    // Inspect client.get_error() after a native receive failure.
}
```

`get_error()` records portable library errors rather than raw native error numbers.
A positive read timeout sets `err_timeout`; TCP EOF clears the read error.
Some legacy I/O methods still have different return-value behavior, so use a
buffer read when the distinction between EOF, timeout, and failure matters.

## Known Limitations

- Base TCP write timeout checks are limited to explicit-size buffer overloads; other overloads and transports have different paths.
- `read_line()` does not apply `read_timeout_ms` and has a fixed 256-byte buffer limit.
- `read(int)` returns an empty string for timeout, EOF or error; use the buffer overload when the distinction matters.
- The current timeout mechanism uses `select()` readiness checks rather than a deadline that bounds an entire multi-step transfer. POSIX `select()` also has descriptor-set limits.
