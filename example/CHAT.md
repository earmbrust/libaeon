# TCP chat example

`chat-server` demonstrates an application-owned, single-threaded TCP chat
service built on `aeon::event_socket_set::poll()` and bounded
`event_socket::queue_write()` operations. It is a simple educational server,
not a hardened internet-facing chat service.

Build with the other examples. From the build tree on Windows:

```powershell
.\Release\chat-server.exe 2300 127.0.0.1
```

On Linux/macOS, run `./chat-server 2300 127.0.0.1` from the build tree.
Connect two terminals using a line-oriented TCP client such as Telnet or
Netcat, then enter text terminated by a newline. Each client receives its
own `guest-N` greeting, and completed lines are broadcast to all clients,
including the sender. CRLF input is accepted. Empty lines are ignored.

The application uses one native TCP listener and one independently owned
socket per peer. Registration is non-owning. A poll callback only marks peers
for removal; sockets are deregistered immediately and physically destroyed
after `poll()` returns, avoiding use-after-free of readiness snapshots.

Defaults: 128 clients, 1024 bytes per incoming line, and a 128 KiB outbound
queue for each client. Clients that send oversized or NUL-containing lines,
or whose outbound queue is full, are disconnected. Queue pressure for one
client does not block the others. TCP can split or coalesce lines arbitrarily;
per-client buffers preserve message boundaries. On shutdown, the server
unregisters and destroys clients before closing its listener.

The example binds to loopback by default. Binding publicly (for example,
`0.0.0.0`) exposes an unauthenticated plaintext service and should only be
done in a trusted environment. No Aeon worker threads are created.
