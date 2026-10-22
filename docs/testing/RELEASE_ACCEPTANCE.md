# Release acceptance

The project is not release-ready. Complete each row with a command and retained
evidence before publication.

| Gate | Current evidence | Status |
|---|---|---|
| Clean configure, build and full tests | Release passes 44 cases. One fresh Debug run reported an unresolved process segfault after a test printed a pass; repeated runs passed. | Open |
| Protocol fuzzing | Clang libFuzzer target added; local execution unavailable. | Open |
| C++ coverage and sanitizer checks | GCC coverage instructions exist; no report or sanitizer result is recorded. | Open |
| Security requirements and threat model | STRIDE and initial OWASP checklist exist; review is limited to the current local service. | Partial |
| Dependency, secret and container scans | Trivy, Gitleaks, CodeQL and container scans have not run. | Open |
| Dashboard and web security testing | No HTTP API or dashboard exists. | Not applicable yet |
| Detection and false-positive evaluation | No detector or labeled corpus exists. | Open |
| Load, soak and failure recovery | Only bounded connection and read-timeout integration cases are tested. | Partial |
| Privacy, retention and data deletion | No persistent player telemetry is collected. Design and verification remain open. | Open |
| Build provenance and SBOM | No release workflow produces attestations or SBOM artifacts. | Open |
