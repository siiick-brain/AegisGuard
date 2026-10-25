# Test environment

The latest local network test run used Windows 11 with CMake 4.4.4 and Ninja
1.13.2. It used MSYS2 UCRT64 GCC 15.2.0 and standalone Asio 1.36.0. OpenSSL
3.6.1 and GoogleTest 1.17.0 were also used along with Google Benchmark 1.9.4.

CMake and Ninja are installed through WinGet, while GCC and OpenSSL are
provided by MSYS2. This audit used absolute CMake and CTest paths because bare
command resolution varied between VS Code terminal sessions. The README
commands require CMake and CTest on the terminal's `PATH` or absolute paths.

Clang/libFuzzer, Docker and Trivy are unavailable locally. Gitleaks,
Cosign and ZAP are not installed either. Prometheus and the
OpenTelemetry Collector are also unavailable. Syft and CodeQL plus Scorecard
are configured through pinned GitHub Actions. The latest remote CI run passed
Debug and Release build/test jobs and generated a source SBOM in Release.
CodeQL result upload and Scorecard analysis failed. No container job or Trivy
result appeared in that run. These runs used commit
`ee16ab46c3686ac15366692d90d02010c84aa759`, not the current local worktree.
CodeQL and Scorecard are **Not verified while the repository is private**.
