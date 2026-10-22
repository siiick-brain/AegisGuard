# Security test plan

## Authorization and scope

Current network tests use an ephemeral loopback TCP listener owned by the test
process. They do not probe public hosts or third-party services. The client uses
a generated self-signed certificate with verification disabled only in tests.

## Current protocol checks

- TLS 1.3 is required; a TLS 1.2-only client is rejected.
- Unknown and revoked credentials are rejected.
- Credentials are checked again for each movement frame.
- Invalid magic, duplicate sequences and excessive movement rates close the
  connection.
- Incomplete authentication and frames are bounded by deadlines.
- Connection count and latest movement state have explicit bounds.
- Shutdown stops accept and tick work, then attempts bounded TLS shutdown for
  authenticated clients.

These checks are custom tests for the game protocol. They do not establish
security certification or cover all possible attack paths.

## OWASP references

The checklist is informed by [ASVS 5.0.0](../security/application-security-checklist.md).
The planned web test cases use the stable [WSTG 4.2](https://owasp.org/www-project-web-security-testing-guide/v42/).
The OWASP Top 10 and API Security Top 10 apply when an HTTP dashboard or API is
implemented. No HTTP target exists today.

OWASP ZAP baseline or active scans have not been run. ZAP testing is deferred
until an isolated local HTTP deployment exists. Active tests will be limited to
that owned test deployment. No findings are inferred from unavailable scans.
