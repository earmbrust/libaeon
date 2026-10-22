# HTTP server example

`http-server` is a minimal **plaintext** HTTP/1.0–HTTP/1.1 demonstration built on
libAeon's application-controlled TCP event poller and bounded send queues.
It is **not** a hardened production web server or an HTTPS implementation.

## Run

```powershell
.\build\Release\http-server.exe 8080 127.0.0.1
```

Open <http://127.0.0.1:8080/> or run:

```powershell
curl.exe -v http://127.0.0.1:8080/health
curl.exe -I http://127.0.0.1:8080/
```

Use Ctrl-C to exit. Use `http-server --help` for argument syntax.

## Supported behavior

- `GET` and `HEAD` for `/`, `/index.html`, and `/health`.
- `404` for unknown paths, `405` for unsupported methods, `505` for unsupported
  HTTP versions, and `400` for malformed HTTP framing/headers.
- HTTP/1.1 keep-alive by default; HTTP/1.0 closes by default unless
  `Connection: keep-alive` is requested.
- Ordered pipelined requests, bounded header memory, a bounded output queue,
  connection limits, and idle-client expiry.
- No request bodies or chunked transfer encoding: positive `Content-Length`
  is rejected with `413`, transfer encoding with `501`, and the connection closes.
- Each response includes `Content-Length` and `Connection` headers.

This example does not implement TLS, streaming request bodies, dynamic routing,
HTTP/2, slowloris protection suitable for internet exposure, or proxy semantics.
The default listener is loopback-only. Socket ownership stays with the example;
no background thread is started by libAeon.
