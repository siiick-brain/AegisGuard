# Release acceptance

The project is not release-ready. Complete each row with a command and retained
evidence before publication.

| Gate | Current evidence | Status |
|---|---|---|
| Clean configure, build and full tests | The latest local CTest run passed 123/123 tests. The Python suite passed 15 tests and two subtests. The coverage report records its earlier 121/121 CTest and 14/14 Python results. Earlier Debug/Release repetitions and the MinGW Asio thread-model fix are in [known failures](KNOWN_FAILURES.md). Other release gates remain open. | Partial |
| Protocol fuzzing | Clang libFuzzer target added; local execution unavailable. | Open |
| C++ coverage and sanitizer checks | Core coverage measured 92.3% line and 80.8% branch against 90% and 80% thresholds. Sanitizer checks remain unverified. | Partial |
| Security requirements and threat model | STRIDE and initial OWASP checklist exist; review is limited to the current local service. | Partial |
| Dependency, secret and container scans | The latest remote CI run had no container job or Trivy result. Dependency-Check and secret scanning remain unconfigured. | Open |
| Dashboard and web security testing | The HTTPS health/status/metrics listener has loopback integration tests. ZAP and dashboard testing have not run. | Partial |
| Detection and false-positive evaluation | Three protocol-rule signals have unit and TLS integration tests. An offline movement heuristic is evaluated on a disjoint seeded holdout of 80 synthetic traces; no real gameplay corpus or representative false-positive evaluation exists. | Partial |
| Load, soak and failure recovery | Only bounded connection and read-timeout integration cases are tested. | Partial |
| Privacy, retention and data deletion | No persistent player telemetry is collected. Design and verification remain open. | Open |
| Build provenance and SBOM | The latest remote Release CI job generated a source SBOM. A tagged release workflow configures keyless Cosign signing, verification and a tamper check; no signed release run is recorded. | Open |
