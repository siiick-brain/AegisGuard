# AegisGuard

AegisGuard is a C++20 authoritative game simulation under development.
Clients provide movement intent. The server validates it and updates player
state on a fixed timestep.

**Status:** The simulation and spatial index are implemented along with an
initial TLS game service and a separate HTTPS status listener. The game service
accepts TLS 1.3 and authenticates a bounded session credential before
forwarding validated movement intent to the fixed-tick simulation. The HTTPS
listener exposes a public health check and mTLS-protected status and metrics
routes. Server output uses JSON Lines and excludes credential and key material.
Milestone 4 adds protocol-rule detection with test-only recommendations.
Milestone 5 adds bounded in-memory replay sessions.
Milestones 6 and 7 add an experimental offline movement-statistics heuristic
and a seeded synthetic holdout evaluation. These features do not use game data
or affect server enforcement.

Milestone 8 adds CI with CodeQL and Scorecard plus source-SBOM workflows. They
have not run on GitHub. Earlier local runs passed 67/68 Release tests and
66/68 Debug tests. After fixing inconsistent
MinGW Asio thread selection the full 70-test CTest suite passed 10/10 runs in
both Debug and Release. Three focused crash-prone tests also passed 20
repetitions each. This resolves the observed local test crash on this
toolchain but does not validate other platforms. A standalone server executable
and loopback integration tests are present; there is no game client or complete
match lifecycle. See the
[requirements matrix](docs/requirements-matrix.md) and
[known failures](docs/testing/KNOWN_FAILURES.md).

## Current implementation

- A server-owned 2D world with configurable bounds and maximum speed on a fixed
  timestep.
- Input validation for player identity and duplicate commands. It also checks
  finite values and movement limits.
- Atomic tick rejection: invalid input does not partially update state or
  advance the tick.
- A dynamic Quadtree with insert/remove/update operations and inclusive range
  queries.
- A seeded Quadtree test that checks random operations against a brute-force
  oracle.
- Google Benchmark workloads for tree operations and brute-force queries.
- A version 2 movement frame with an explicit HELLO, an aim angle and an
  untrusted position claim used only for anomaly checks. The authoritative
  simulation never accepts client position as state.
- Movement telemetry checks for excessive speed, teleport distance and aim
  angular snap rate with latency, jitter and fast-turn tolerance tests.
- A bounded session store with OpenSSL-generated credentials. It supports
  expiry and revocation with concurrent access protection.
- An Asio listener that requires TLS 1.3 and validates session credentials. It
  limits connections and input rates. It rejects replayed frames and applies
  read deadlines before feeding the simulation on its fixed tick. Accepted
  movement intents enter a bounded per-player FIFO. A shared per-IP token
  bucket preserves rate state across reconnects.
- A separate HTTPS listener for `/healthz`, `/status` and `/metrics`.
  `/healthz` is unauthenticated; status and metrics require a trusted client
  certificate. JSON Lines logs contain structured lifecycle and HTTP events
  without credential or key fields.
- A rule detector for out-of-range movement and replayed sequences plus rate-limit
  violations. Findings include a reason and rolling count. Recommendations can
  escalate from review to throttle or disconnect. The detector applies only
  `observe` and existing protocol rejections remain in force.
- A bounded in-memory replay session that records accepted simulation inputs
  and validates the recorded ticks by replaying them from the initial state.
- A versioned movement-feature extractor and configurable repeated-input
  heuristic for offline replay analysis. Seeded synthetic traces exercise a
  disjoint holdout set. Findings remain observe-only; real gameplay has not
  been evaluated.
- A libFuzzer target for the untrusted movement-frame decoder. It is optional
  and has not yet been run in this Windows toolchain because Clang is absent.

The simulation does not include collision handling or health changes. It also
has no persistence. The Quadtree returns point-query candidates; it does not
perform exact collision tests. Session issuance and player provisioning remain outside
the network service. The service defaults to loopback and has no production
certificate provisioning or key rotation workflow. Do not expose it to public
traffic. See the [protocol documentation](docs/protocol.md) and [architecture
overview](docs/architecture/overview.md). The [threat model](docs/threat-model/stride.md)
also describes the current boundaries.

The rule scope and thresholds are documented with evidence fields and
evaluation limits in the
[rule detection notes](docs/detection/rule-detection.md).
Feature definitions and synthetic test results are documented with their
limits in the
[movement statistics notes](docs/detection/movement-statistics.md).
Replay contents and bounds are documented with retention and current
limitations in the
[replay documentation](docs/telemetry/replay.md).

## Build and test

Requirements: CMake 3.25 or newer with Ninja and Git. You also need a C++20
compiler plus standalone Asio and OpenSSL 3.0 or newer with SSL and Crypto
libraries. The first configure fetches GoogleTest 1.17.0.

For the MSYS2 UCRT64 toolchain open an MSYS2 UCRT64 terminal and
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

These commands were verified with CMake 4.4.4 and Ninja 1.13.2 on MSYS2 GCC
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
workloads and their limitations. Its JSON output is saved under
[`benchmarks/results`](benchmarks/results).

## Build dependencies

GoogleTest 1.17.0 is used for unit tests under its BSD 3-Clause license.
Google Benchmark 1.9.4 is optional under Apache License 2.0. CMake fetches
these tagged releases into the ignored build directory. Standalone Asio 1.36.0
uses Boost Software License 1.0. OpenSSL 3.6.1 uses Apache License 2.0.

## Testing and security evidence

- [Test strategy and catalogue](docs/testing/TEST_STRATEGY.md)
- [Test matrix](docs/testing/TEST_MATRIX.md)
- [Protocol fuzzing](docs/testing/PROTOCOL_FUZZING.md)
- [OWASP assessment](docs/security/owasp-assessment.md)
- [Security requirements matrix](docs/security/requirements-verification.md)
- [CI and software supply chain](docs/operations/ci-and-supply-chain.md)

The TLS tests use an ephemeral self-signed certificate. The test client
disables certificate verification. These tests verify local service behavior
but not certificate issuance or production trust configuration. The libFuzzer
harness is configured for Clang on Linux or macOS. It has not run in the
current Windows environment. CodeQL and Scorecard workflows are configured
but have not run remotely. ZAP and Trivy remain deferred. So do
Dependency-Check and Prometheus. OpenTelemetry is also deferred until
supported targets and inputs exist.

## Design and project status

- [Architecture overview](docs/architecture/overview.md)
- [C++20 and CMake decision](docs/architecture/decisions/0001-cpp20-cmake.md)
- [TCP and TLS transport decision](docs/architecture/decisions/0002-tcp-tls-transport.md)
- [Protocol and network service](docs/protocol.md)
- [Spatial benchmark report](docs/operations/spatial-benchmarks.md)
- [STRIDE threat model](docs/threat-model/stride.md)
- [Requirements and milestone gates](docs/requirements-matrix.md)

There is no real-player evaluation data. The benchmark results describe only
the checked-in synthetic workloads on one machine. Detection quality and
false-positive rates have not been measured. Memory use and operational
security have not been measured either.
