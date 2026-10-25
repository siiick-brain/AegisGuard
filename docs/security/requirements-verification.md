# Security requirements and verification

This matrix maps standards to project work. It does not claim certification or
compliance. It does not claim maturity.

| Source | Project use | Current implementation or evidence | Status |
|---|---|---|---|
| OWASP Top 10 | Guide review of the HTTPS health/status/metrics surface for access control, configuration, injection and logging. | The listener has three GET routes and no browser dashboard or general-purpose API. | Partial |
| OWASP API Security Top 10 | Check object/function authorization, authentication and resource consumption. | Status and metrics require a trusted client certificate; health is public. All three routes have loopback tests; broad API testing is not performed. | Partial |
| OWASP ASVS 5.0.0 | Checklist for transport, authentication, input validation, resource limits and future application controls. | [Application security checklist](application-security-checklist.md) and TLS tests. | Partial |
| OWASP WSTG 4.2 | Plan authorized tests for supported HTTP surfaces. | [Security test plan](../testing/SECURITY_TEST_PLAN.md); HTTPS status routes exist, but ZAP is not run. The custom game protocol is outside HTTP scope. | Partial |
| STRIDE | Identify spoofing, tampering, repudiation, information disclosure, denial of service and privilege escalation risks. | [Threat model](../threat-model/stride.md), updated for the TLS listener. | In use |
| NIST SSDF | Organize secure development, review, testing and release evidence. | The latest remote CI run passed Debug and Release build/test jobs and generated a source SBOM in Release. CodeQL result upload and Scorecard analysis failed. | CI jobs verified for that pushed commit; CodeQL and Scorecard **Not verified while the repository is private** |
| MITRE ATT&CK | Map only adversarial behaviors supported by implemented detectors. | The detector reports protocol violations; no ATT&CK technique mapping is justified by these signals. | Deferred |
| CIS Benchmarks | Apply hardening guidance to deployment technologies selected later. | Dockerfile and Compose hardening are configured; no CIS assessment or deployment run has been performed. | Open |
| OpenSSF Scorecard | Review repository and workflow security after CI exists. | The latest listed Scorecard run failed during supply-chain analysis. | **Not verified while the repository is private** |
| SLSA concepts | Add verifiable build provenance when a release pipeline produces artifacts. | The workflow produces a source/build SBOM artifact, but there is no release binary or provenance attestation. | Deferred |
| Trivy and OWASP Dependency-Check | Scan supported dependency manifests, containers and deployment configuration without redundant coverage. | No container job or Trivy result appeared in the latest remote CI run. Dependency-Check remains unconfigured because there is no supported C/C++ lockfile; the project uses CMake `FetchContent` and system packages. | Trivy not verified; Dependency-Check deferred |

OWASP ASVS 5.0.0 and the OWASP API Security project documentation use
CC BY-SA 4.0. The OWASP WSTG project also uses CC BY-SA 4.0. The project links
to these resources and records its adaptations without copying their
requirements verbatim.
