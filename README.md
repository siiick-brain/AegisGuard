# AegisGuard

AegisGuard is a C++20 server-authoritative game simulation under development.
Clients provide movement intent; the server validates it and updates player
state on a fixed timestep.

**Status:** The simulation, spatial index and initial TLS game service are
implemented. The service accepts TLS 1.3, authenticates a bounded session
credential and sends validated movement intent into the fixed-tick simulation.
All 44 tests pass in the Release build. Milestone 3 remains open because one
fresh Debug test run reported an intermittent process segfault after a TLS
shutdown test printed a pass. Repeated Debug runs have not reproduced it. See
the [requirements matrix](docs/requirements-matrix.md) for current status.

## Current implementation

- A server-owned 2D world with configurable bounds, maximum speed, and fixed
  timestep.
- Input validation for player identity, duplicate commands, finite values, and
  movement limits.
- Atomic tick rejection: invalid input does not partially update state or
  advance the tick.
- A dynamic Quadtree with insert, remove, update, and inclusive range queries.
- A seeded Quadtree test that checks random operations against a brute-force
  oracle.
- Google Benchmark workloads for tree operations and brute-force queries.
- A bounded version 1 movement frame, sequence guard, and token-bucket limiter.
- A bounded session store with OpenSSL-generated credentials, expiry, revocation
  and concurrent access protection.
- An Asio listener that requires TLS 1.3, validates session credentials, limits
  connections and input rates, rejects replayed frames, applies read deadlines
  and feeds the simulation on its fixed tick.
- A libFuzzer target for the untrusted movement-frame decoder. It is optional
  and has not yet been run in this Windows toolchain because Clang is absent.

The simulation does not include collision handling, health changes or
persistence. The Quadtree returns point-query candidates; it does not perform
exact collision tests. Session issuance and player provisioning remain outside
the network service. The service defaults to loopback and has no production
certificate provisioning or key rotation workflow. Do not expose it to public
traffic. See the [protocol documentation](docs/protocol.md), [architecture
overview](docs/architecture/overview.md) and [threat model](docs/threat-model/stride.md)
for its current boundaries.

## Build and test

Requirements: CMake 3.25 or newer, Ninja, Git, a C++20 compiler, standalone Asio,
and OpenSSL 3.0 or newer with SSL and Crypto libraries. The first configure
fetches GoogleTest 1.17.0.

For the MSYS2 UCRT64 toolchain used here, open an MSYS2 UCRT64 terminal and
install the network and crypto development packages:

```bash
pacman -S --needed mingw-w64-ucrt-x86_64-asio mingw-w64-ucrt-x86_64-openssl
```

From the repository root in PowerShell:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

These commands were verified with CMake 4.4.4, Ninja 1.13.2, and MSYS2 GCC
15.2.0. A successful test run does not establish production readiness or
detection accuracy.

Build and run the spatial benchmarks in Release mode:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DAG_BUILD_BENCHMARKS=ON
cmake --build build --parallel
.\build\benchmarks\aegisguard_spatial_benchmarks.exe --benchmark_min_time=0.1s --benchmark_repetitions=5 --benchmark_report_aggregates_only=true --benchmark_out=build\spatial-raw.json --benchmark_out_format=json
python tools\evaluation\sanitize_benchmark_json.py build\spatial-raw.json benchmarks\results\spatial-gcc-15.2.0.json
```

Google Benchmark 1.9.4 is fetched when benchmarks are enabled. The checked-in
[benchmark report](docs/operations/spatial-benchmarks.md) describes the tested
workloads and limitations. Its JSON output is saved under
[`benchmarks/results`](benchmarks/results).

## Build dependencies

GoogleTest 1.17.0 is used for unit tests under its BSD 3-Clause license.
Google Benchmark 1.9.4 is optional and uses Apache License 2.0. CMake fetches
these tagged releases into the ignored build directory. Standalone Asio 1.36.0
uses Boost Software License 1.0. OpenSSL 3.6.1 uses Apache License 2.0. The
repository also contains the requested Humanizer writing skill under its MIT
license; it is development guidance, not a runtime dependency.

## Testing and security evidence

- [Test strategy and catalogue](docs/testing/TEST_STRATEGY.md)
- [Test matrix](docs/testing/TEST_MATRIX.md)
- [Protocol fuzzing](docs/testing/PROTOCOL_FUZZING.md)
- [OWASP assessment](docs/security/owasp-assessment.md)
- [Security requirements matrix](docs/security/requirements-verification.md)

The TLS tests use an ephemeral self-signed certificate and disable certificate
verification in the test client. They verify local service behavior, not
certificate issuance or production trust configuration. The libFuzzer harness
is configured for Clang on Linux or macOS; it has not been executed in the
current Windows environment. ZAP, Trivy, CodeQL, Prometheus and OpenTelemetry
are deferred to the milestones where their targets exist.

## Design and project status

- [Architecture overview](docs/architecture/overview.md)
- [C++20 and CMake decision](docs/architecture/decisions/0001-cpp20-cmake.md)
- [TCP and TLS transport decision](docs/architecture/decisions/0002-tcp-tls-transport.md)
- [Protocol and network service](docs/protocol.md)
- [Spatial benchmark report](docs/operations/spatial-benchmarks.md)
- [STRIDE threat model](docs/threat-model/stride.md)
- [Requirements and milestone gates](docs/requirements-matrix.md)

There is no real-player evaluation data. The benchmark results describe only
the checked-in synthetic workloads on one machine. Detection quality,
false-positive rates, memory use, and operational security have not been
measured.
