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

Earlier full-suite runs had intermittent network-test process crashes. The
current worktree registers 70 GoogleTest cases. After unifying MinGW Asio
thread-backend selection across translation units, the complete suite passed
10/10 consecutive runs in both Debug and Release. Three previously crash-prone
network tests passed 20 focused Debug repetitions each; Python utility tests
passed 2/2. The root cause and fix evidence are recorded in
[known failures](../testing/KNOWN_FAILURES.md). These tests verify the listed
local controls. They are not a penetration test or an independent security
review.

No confirmed OWASP findings have been recorded. ZAP baseline and active scans
were not run because there is no HTTP dashboard or API. CodeQL and Scorecard
workflows are configured but have not run on GitHub. Trivy,
Dependency-Check, secret scanning and container checks are not configured;
there is no supported C++ lockfile, container image or infrastructure target.
Missing scans are not clean results.

## Deferred work

- Revisit OWASP Top 10 and API Security Top 10 when an HTTP surface exists.
- Run ZAP only against an isolated local dashboard/API deployment, with scope
  recorded separately for baseline and active tests.
- Re-evaluate Dependency-Check and Trivy when a supported dependency manifest,
  container image or infrastructure target is added.
- Evaluate OWASP CRS only if an HTTP reverse proxy or WAF becomes part of the
  deployment. It does not protect the custom game protocol.
- Keep OWASP Juice Shop isolated as an optional training target; it is not part
  of the AegisGuard deployment.
