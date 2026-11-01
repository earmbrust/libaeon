# libAeon testing

libAeon uses Google Test for unit and integration tests and CTest to run the
configured test executables. Tests are disabled in the default library build.
Google Test is fetched when `BUILD_TESTING=ON`.

## Configure and run

On Linux or macOS:

```sh
cmake -S . -B build-tests -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build-tests --parallel
ctest --test-dir build-tests --output-on-failure
```

On Windows with Visual Studio 2026:

```powershell
cmake -S . -B build-vs2026-tests -G "Visual Studio 18 2026" -A x64 -DBUILD_TESTING=ON
cmake --build build-vs2026-tests --config Release
ctest --test-dir build-vs2026-tests -C Release --output-on-failure
```

The core socket tests can also run with `-DLIBAEON_ENABLE_EVENTS=OFF`.
Event socket tests require event support. TLS tests require OpenSSL and
`-DENABLE_SSL=ON`. Examples are enabled separately with
`-DLIBAEON_BUILD_EXAMPLES=ON`.

## Test executables

CTest registers these targets when testing is enabled:

- `AeonTests`: public TCP/UDP, address, resolver, event, ownership and example
  integration tests. Event-specific tests are conditional on event support.
- `AeonInternalTests`: implementation-level UDP tests against the static library.
- `AeonSharedConsumer` and `AeonStaticConsumer`: independent C++20 consumers
  exercising public headers and shared/static linkage.
- `AeonTlsLiveShared` and `AeonTlsLiveStatic`: real loopback TLS tests, included
  when SSL/TLS is enabled.

To run only the public test executable:

```sh
ctest --test-dir build-tests -R '^AeonTests$' --output-on-failure
```

To select individual Google Test cases, run the test executable directly:

```sh
./build-tests/aeon_tests --gtest_list_tests
./build-tests/aeon_tests --gtest_filter='AddressTest.*'
```

On multi-configuration Windows builds, use the appropriate configuration
subdirectory (for example, `build-vs2026-tests\Release\aeon_tests.exe`).
The `--gtest_filter` option belongs to the Google Test executable, not CTest.

## Continuous integration

GitHub Actions builds and tests the library on Windows, Linux and macOS,
including selected x86, x64 and ARM targets. Optional TLS and event-disabled
configurations run separately. A successful build does not imply coverage of
all runtime or performance conditions.

Tests should use OS-assigned ports and bounded waits where practical. Avoid
assuming that TCP reads correspond to application message boundaries or that
UDP datagrams will arrive in order. Socket collections may be non-owning;
tests must respect the lifetime of registered sockets.
