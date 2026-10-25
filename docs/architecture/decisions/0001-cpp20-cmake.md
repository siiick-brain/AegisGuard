# ADR 0001: C++20 and CMake as the server baseline

- Status: Accepted
- Date: 2026-10-22

## Context

AegisGuard's core is a systems-oriented authoritative simulation with spatial
queries and untrusted-input boundaries. It needs explicit compiler support
and repeatable builds. It also needs test integration and a path to CI across
platforms. The workspace was empty and not version-controlled at inspection
time. It currently has MSYS2 GCC 15.2.0 but no CMake or Ninja. MSVC and a test
runner are also absent.

## Decision

- Use C++20 for server code and CMake 3.25 or newer as the build system.
- Use CTest to expose the test suite.
- Keep the initial simulation independent of networking and external services.
- Record toolchain and build configuration in project files and CI once Milestone
  1 establishes and verifies them.
- Do not select a networking or cryptography dependency before its
  requirements and maintenance/security posture are reviewed. Apply the same
  review to ML dependencies.

## Rationale

C++20 supports the requested systems-engineering scope. CMake and CTest provide
a portable build/test interface suitable for the available GCC toolchain and
future CI. Deferring networking prevents transport assumptions from shaping the
simulation core.

## Consequences and validation

- Local C++ builds cannot be validated yet because CMake is absent. This ADR is
  a strategy. It is not evidence of a reproducible working build.
- Milestone 1 installed CMake 4.4.4 and Ninja 1.13.2 for the current user.
  Configure and build were verified along with all 9 GoogleTest cases on
  MSYS2 GCC 15.2.0.
- CI should later validate at least one supported compiler configuration; a
  second compiler/platform is desirable before claiming portability.
- The minimum CMake version and supported compiler versions must be revisited if
  the selected test framework or dependencies require a different baseline.
