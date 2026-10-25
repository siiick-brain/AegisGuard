# Test environment

The latest local network test run used Windows 11 with CMake 4.4.4 and Ninja
1.13.2. It used MSYS2 UCRT64 GCC 15.2.0 and standalone Asio 1.36.0. OpenSSL
3.6.1 and GoogleTest 1.17.0 were also used along with Google Benchmark 1.9.4.

CMake and Ninja are installed through WinGet, while GCC and OpenSSL are
provided by MSYS2. This audit used absolute CMake and CTest paths because bare
command resolution varied between VS Code terminal sessions. The README
commands require CMake and CTest on the terminal's `PATH` or absolute paths.

Clang/libFuzzer and Docker are unavailable locally. Trivy and Gitleaks are not
installed. Cosign and ZAP are not installed either. Prometheus and the
OpenTelemetry Collector are also unavailable. Syft and CodeQL plus Scorecard
are configured through pinned GitHub Actions. No remote workflow run has
completed. No scan or SBOM result is claimed. No Scorecard result is claimed
either.
