# OWASP security assessment

## Scope and method

The assessment covers the local TLS game service, its movement protocol and
the separate HTTPS listener for health, status and metrics. Tests run against
loopback sockets with an ephemeral certificate authority. They cover TLS
version rejection, credential validation and revocation. They also test
malformed frames, replay, rate limits, connection limits, incomplete-frame
deadlines, fixed-tick dispatch and client-certificate authorization for
status and metrics.

The process follows the trust-boundary and threat review in the
[STRIDE threat model](../threat-model/stride.md). The ASVS-informed checklist is
in [application security checklist](application-security-checklist.md).
OWASP's [WSTG 4.2](https://owasp.org/www-project-web-security-testing-guide/v42/)
is the selected stable web-testing reference. Only the limited HTTPS status
surface is in scope for HTTP checks. The game protocol is not HTTP and these
web tests do not apply to it.

## Results

The current worktree registers 125 CTest cases. The latest complete Debug and
Release CTest runs passed 125/125. Standalone Python unittest discovery passed
14/14, and the CTest pytest suite passed 28 tests and two subtests. Trusted and
untrusted client certificates were both checked by the server smoke test. The
earlier MinGW Asio
thread-backend mismatch and its repeated Debug/Release test evidence are
recorded in
[known failures](../testing/KNOWN_FAILURES.md). These tests verify the listed
local controls. They are not a penetration test or an independent security
review.

No independent OWASP findings have been recorded. ZAP baseline and active
scans were not run. The latest remote CI run passed Debug and Release
build/test jobs and generated a source SBOM in Release. No container job or
Trivy result appeared in that run. The latest CodeQL result upload and
Scorecard analysis failed; both are **Not verified while the repository is
private**. Dependency-Check remains unconfigured because there is no
supported C++ lockfile. Secret scanning also remains unconfigured. A
configured scan is not a clean scan result.

## Deferred work

- Review the three read-only HTTPS routes against the relevant OWASP Top 10
  and API Security guidance. Do not treat the listener as a general-purpose
  web application.
- Run ZAP only against an isolated local HTTPS deployment. Record scope
  separately for baseline and active tests.
- Re-evaluate Dependency-Check when a supported dependency manifest is added.
  Trivy image scanning is configured but has no recorded container run.
- Evaluate OWASP CRS only if an HTTP reverse proxy or WAF becomes part of the
  deployment. It does not protect the custom game protocol.
- Keep OWASP Juice Shop isolated as an optional training target; it is not part
  of the AegisGuard deployment.
