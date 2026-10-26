# Release acceptance

The project is not release-ready. Complete each row with a command and retained
evidence before publication.

| Gate | Current evidence | Status |
|---|---|---|
| Clean configure, build and full tests | The latest complete Debug and Release CTest runs passed 125/125 tests. The fresh coverage CTest run also passed 125/125. Standalone Python unittest discovery passed 14/14; the CTest pytest suite passed 28 tests and two subtests. A Release per-IP test timing failure was corrected by waiting for each tick's velocity before sending the next intent; 50 focused repeats and the full suite then passed. A separate health-check timeout did not reproduce in the isolated test, three direct repetitions or a subsequent full run. Other release gates remain open. | Partial |
| Protocol fuzzing | Three Clang libFuzzer targets and a bounded Ubuntu CI smoke job are configured; local execution is unavailable and the remote job has not run. | Open |
| C++ coverage and sanitizer checks | A fresh full coverage CTest run passed 125/125; gcovr measured 91.5% lines, 81.0% branches and 97.5% functions against 90%/80% thresholds. Earlier timeout-marked coverage tests passed after the dedicated build was cleaned and rebuilt. Sanitizer checks remain unverified. | Partial |
| Security requirements and threat model | STRIDE and initial OWASP checklist exist; review is limited to the current local service. | Partial |
| Dependency, secret and container scans | The latest remote CI run had no container job or Trivy result. Dependency-Check and secret scanning remain unconfigured. | Open |
| Dashboard and web security testing | The HTTPS health/status/metrics listener has loopback integration tests. ZAP and dashboard testing have not run. | Partial |
| Detection and false-positive evaluation | Eight protocol/movement/rate-limit signals have unit and TLS integration tests. An offline movement heuristic is evaluated on a disjoint seeded holdout of 80 synthetic traces; no real gameplay corpus or representative false-positive evaluation exists. | Partial |
| Load, soak and failure recovery | Only bounded connection and read-timeout integration cases are tested. | Partial |
| Privacy, retention and data deletion | No persistent player telemetry is collected. Design and verification remain open. | Open |
| Build provenance and SBOM | The latest remote Release CI job generated a source SBOM. A tagged release workflow configures keyless Cosign signing, verification and a tamper check; no signed release run is recorded. | Open |
