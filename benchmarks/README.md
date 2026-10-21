# TCP write queue benchmark

The optional `aeon_write_queue_benchmark` executable measures end-to-end
loopback throughput through `event_socket::queue_write()` and
`event_socket_set::poll()`. It also reports how many attempts encountered the
bounded 1 MiB outgoing queue. It checks the received data for corruption.

The benchmark has no GoogleTest dependency, uses one application thread, and
is not part of the CTest pass/fail suite. Numbers are environmental and are **not**
minimum performance guarantees.

Configure and build on Windows (Visual Studio multi-config):

```powershell
cmake -S . -B .\build -DLIBAEON_BUILD_BENCHMARKS=ON
cmake --build .\build --config Release --target aeon_write_queue_benchmark
.\build\Release\aeon_write_queue_benchmark.exe 33554432 4096
```

Configure and build on Linux/macOS:

```sh
cmake -S . -B build -DLIBAEON_BUILD_BENCHMARKS=ON -DBUILD_TESTING=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build --target aeon_write_queue_benchmark
./build/aeon_write_queue_benchmark 33554432 4096
```

The two arguments are total bytes (default 32 MiB) and producer chunk size
(default 4 KiB; maximum 64 KiB). For comparisons, use identical build options,
network stack settings, arguments, and machine load. Repeat runs to account
for operating-system scheduling variation.
