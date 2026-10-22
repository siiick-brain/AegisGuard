# Test environment

The latest local network test run used Windows 11, CMake 4.4.4, Ninja 1.13.2,
MSYS2 UCRT64 GCC 15.2.0, standalone Asio 1.36.0, OpenSSL 3.6.1, GoogleTest
1.17.0 and Google Benchmark 1.9.4.

CMake and CTest are installed through WinGet but were not on the PowerShell
process `PATH` during this session. Tests were run using the executable path
recorded in `build/CMakeCache.txt`. A normal terminal should have the CMake
`bin` directory on `PATH` before using the README commands.

Clang/libFuzzer, Docker, Trivy, Gitleaks, Cosign, ZAP, Prometheus and the
OpenTelemetry Collector are unavailable locally. Syft, CodeQL and Scorecard
are configured through pinned GitHub Actions, but no remote workflow run has
completed. No scan, SBOM or Scorecard result is claimed yet.
