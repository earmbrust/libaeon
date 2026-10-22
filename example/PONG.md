# UDP ASCII Pong (two players)

M5b is a small multiplayer UDP game that exercises Aeon's destination-aware
UDP sending and application-controlled event polling. The server owns the
ball, paddle positions, score, and simulation clock. Clients only send input.

Build with CMake (`BUILD_TESTING=ON` for integration tests) and run:

```text
pong-server 2302
pong-client 127.0.0.1 2302
pong-client 127.0.0.1 2302
```

Launch the clients in separate terminal windows. Press **W** to move up,
**S** to move down, **X** to stop and **Q** to quit. The first player controls
the left paddle; the second controls the right. A third player receives `FULL`.
Ctrl+C stops the server. ANSI-capable terminals render the ASCII field.

The protocol sends one bounded ASCII command per UDP datagram: `J` joins,
`U`/`D`/`N` selects paddle direction, `H` keeps a seat alive, and `Q` quits.
The server replies `W 1` or `W 2` on joining and broadcasts `S` snapshots
containing sequence, paddle positions, ball position, score, and occupied-seat
bitmask. The client ignores snapshots with an older sequence number.

The simulation runs at 10 ticks/second. Idle seats expire after approximately
five seconds. UDP is connectionless: messages and snapshots may be dropped or
reordered, and this illustrative protocol has no authentication or encryption.
Do not expose the demo on an untrusted network without transport security,
identity protection, and rate limiting.

The game introduces `udp_socket::write_to(address, data, size)`, which sends to
an explicit recipient without overwriting the socket's remembered reply
address. It is particularly useful for UDP servers with more than one peer.
No Aeon background threads, polling API changes, or socket ownership changes.
