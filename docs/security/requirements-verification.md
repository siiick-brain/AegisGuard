# Security requirements and verification

This matrix maps standards to project work. It does not claim certification,
compliance or maturity.

| Source | Project use | Current implementation or evidence | Status |
|---|---|---|---|
| OWASP Top 10 | Guide future HTTP dashboard review for access control, configuration, injection and logging. | No HTTP application exists. | Deferred |
| OWASP API Security Top 10 | Check object/function authorization, authentication and resource consumption for a future API. | No administrative API exists. | Deferred |
| OWASP ASVS 5.0.0 | Checklist for transport, authentication, input validation, resource limits and future application controls. | [Application security checklist](application-security-checklist.md) and TLS tests. | Partial |
| OWASP WSTG 4.2 | Plan authorized tests if an HTTP dashboard/API is added. | [Security test plan](../testing/SECURITY_TEST_PLAN.md); no web target to test. | Planned |
| STRIDE | Identify spoofing, tampering, repudiation, information disclosure, denial of service and privilege escalation risks. | [Threat model](../threat-model/stride.md), updated for the TLS listener. | In use |
| NIST SSDF | Organize secure development, review, testing and release evidence. | Requirements traceability is present; CI controls are not. | Partial |
| MITRE ATT&CK | Map only adversarial behaviors supported by implemented detectors. | Detection does not exist yet. | Deferred |
| CIS Benchmarks | Apply hardening guidance to deployment technologies selected later. | No container or cloud deployment exists. | Deferred |
| OpenSSF Scorecard | Review repository and workflow security after CI exists. | Not run; there are no workflows to assess. | Deferred |
| SLSA concepts | Add verifiable build provenance when a release pipeline produces artifacts. | No release pipeline or provenance artifact exists. | Deferred |

OWASP ASVS 5.0.0 and the OWASP API Security project documentation use
CC BY-SA 4.0. The OWASP WSTG project also uses CC BY-SA 4.0. The project links
to these resources and records its adaptations without copying their
requirements verbatim.
