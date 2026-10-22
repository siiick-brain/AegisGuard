# OWASP security assessment

## Scope and method

The assessment currently covers the local TLS game service and its movement
protocol. Tests run against loopback sockets in the C++ test process using an
ephemeral self-signed certificate. They include TLS version rejection,
credential validation and revocation, malformed frames, replay, rate limits,
connection limits, incomplete-frame deadlines and server-tick dispatch.

The process follows the trust-boundary and threat review in the
[STRIDE threat model](../threat-model/stride.md). The ASVS-informed checklist is
in [application security checklist](application-security-checklist.md).
OWASP's [WSTG 4.2](https://owasp.org/www-project-web-security-testing-guide/v42/)
is the selected stable web-testing reference. Its HTTP scenarios do not apply
to the current game-protocol listener.

## Results

The Release CTest suite passes 44 tests, including 12 TLS service integration
cases. One fresh Debug run reported an unresolved process segfault after the
TLS shutdown test printed a pass. Repeated Debug runs did not reproduce it, so
Milestone 3 remains open. These tests verify the listed local controls. They
are not a penetration test or an independent security review.

No confirmed OWASP findings have been recorded. ZAP baseline and active scans
were not run because there is no HTTP dashboard or API. Trivy, Dependency-Check,
CodeQL, secret scanning and container checks were not run. Missing scans are
not clean results.

## Deferred work

- Revisit OWASP Top 10 and API Security Top 10 when an HTTP surface exists.
- Run ZAP only against an isolated local dashboard/API deployment, with scope
  recorded separately for baseline and active tests.
- Evaluate Dependency-Check alongside Trivy before enabling overlapping CI
  scanners.
- Evaluate OWASP CRS only if an HTTP reverse proxy or WAF becomes part of the
  deployment. It does not protect the custom game protocol.
- Keep OWASP Juice Shop isolated as an optional training target; it is not part
  of the AegisGuard deployment.
