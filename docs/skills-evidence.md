# Skills evidence map

This map distinguishes a script step from recorded verification. A configured
workflow or test name is not a passing result. No skill is considered
demonstrated unless its script step records **PASS** in
`artifacts/skills-report.md`.

The local run on 2026-10-25 recorded 13 **PASS** results, three **BLOCKED**
results and one **Not deployed** result. The script exited with code 0 in
normal mode and recorded no **FAIL** results. Docker and Trivy were
**BLOCKED** because the local tools were unavailable. Cloudflare remains
unconfigured because no domain or active zone is available.
The gate's build step passed 123 CTest cases. Its Python step ran 11 tests in
4.519 seconds.

| Skill | Script step | Test, workflow or file | Current status |
|---|---:|---|---|
| C++ and CMake build | 1 | `cmake --build`; 123 CTest cases; 15 Python tests and two subtests | PASS locally; the latest remote Debug and Release build/test jobs passed |
| Server-authoritative movement | 2 | `NetworkServiceTest.ForgedPositionRejected`; `NetworkServiceTest.AuthenticatedMovementUsesServerFixedTicks` | PASS locally |
| Quadtree spatial queries and oracle | 3 | `QuadtreeTest.MatchesBruteForceAcrossSeededOperationSequence`; 3 live benchmark repetitions; rendered 20-repetition graph | PASS locally; this does not claim a complete physics collision system |
| Speed anomaly detection | 4 | `NetworkServiceTest.SpeedCheatRejected` | PASS locally |
| Teleport anomaly detection | 5 | `NetworkServiceTest.TeleportCheatRejected` | PASS locally |
| Aim snap detection | 6 | `NetworkServiceTest.AimSnapCheatRejected` | PASS locally |
| Unit and integration testing | 1 | Full CTest suite and Python smoke tests | PASS locally |
| Flood limiting and recovery | 7 | `NetworkServiceTest.FloodRateLimitRejected`; `NetworkServiceIpRateRecoveryTest.RecoversAfterPerIpFlood` | PASS locally |
| Structured JSON logging | 8 | `ServerSmokeTests.test_json_logs_parse_and_exclude_secrets` | PASS; 100 records parsed with no sensitive fields |
| TLS client certificates | 9 | `ServerSmokeTests.test_valid_token_admin_mtls_and_signal_shutdown` checks trusted and untrusted clients | PASS; trusted client returned 200 and untrusted client was rejected |
| Digital signatures for server builds | 10 | OpenSSL signature verification and modified-binary rejection; keyless workflow in [release-signing.yml](../.github/workflows/release-signing.yml) | Local signature check PASS; keyless workflow pending a tagged run |
| Secure key provisioning and rotation | 11 | [setup-demo-secrets.ps1](../scripts/setup-demo-secrets.ps1); `ServerConfigTest.ExpandsLocalAppDataForProvisioningPaths`; `SessionStoreTest.RotatesCredentialsAndRejectsThePreviousCredential` | PASS; keys are generated under `%LOCALAPPDATA%\AegisGuard\secrets` with owner-only permissions and credential revocation checked locally |
| Docker hardening | 12 | Compose build/run and `docker inspect`; [container workflow](../.github/workflows/ci.yml) | BLOCKED locally; no container job appeared in the latest remote CI run |
| HTTPS health and protected status | 13 | Curl against `/healthz` and mTLS-protected `/status`; `ServerSmokeTests` verifies the `AEGISGUARD_STATUS_HOSTNAME` TLS-name override and certificate mismatch rejection | BLOCKED locally because Docker is unavailable; Python HTTPS/mTLS and hostname checks passed |
| Threat modeling | 14 | [STRIDE threat model](./threat-model/stride.md), attack trees and control-to-test map | PASS; 8 mapped CTest names checked against inventory |
| Cloudflare DNS, TLS and WAF | 15 | [Cloudflare deployment notes](./operations/cloudflare.md) | Not deployed; no domain or active zone is available |
| Trivy image scanning | 16 | Trivy HIGH/CRITICAL scan of `aegisguard:local` | BLOCKED locally; no container scan appeared in the latest remote CI run |
| Performance benchmarking | 3 | 3 live benchmark repetitions; [20-repetition p50/p95/p99 graph](../benchmarks/results/spatial-query-percentiles.svg) | PASS locally |
| SBOM and CI gates | CI | [CI workflow](../.github/workflows/ci.yml) | Latest remote run passed Debug and Release build/test jobs; source SBOM succeeded in Release |

## Scope notes

- The game protocol is a custom TCP protocol and is outside Cloudflare HTTP
  WAF and rate-limit coverage.
- A keyless Cosign run identifies the signing GitHub workflow. It does not
  establish a trusted publisher identity.
- Demo TLS private keys are restricted by the provisioning script and stored
  outside the repository under `%LOCALAPPDATA%\AegisGuard\secrets`. The
  standalone server writes its reusable player token as plaintext and
  restricts the token file to the current user. The smoke test checks these
  permissions.
- Docker, Trivy, Cloudflare and keyless release evidence must be reported from
  actual runs. A local script result of **BLOCKED** is not a pass.
