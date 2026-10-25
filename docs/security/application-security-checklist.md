# Application security checklist

This checklist adapts verification concerns from OWASP ASVS 5.0.0 to the
implemented game service and its limited HTTPS health/status listener. It is a
development aid, not an ASVS assessment or claim of compliance. The HTTPS
listener is not a general-purpose administrative API or browser application.

| Concern | Project check | Evidence | Status |
|---|---|---|---|
| Transport security | Require TLS 1.3; reject TLS 1.2-only clients. | `NetworkServiceTest.RejectsTlsTwelveClients` | Tested locally |
| Authentication | Validate a fixed-size credential and bind it to an existing server-owned player. | TLS integration tests and `SessionStoreTest` | Tested locally |
| Session lifecycle | Expire and revoke credentials; revalidate active sessions. | Session store tests and `NetworkServiceTest.RechecksCredentialRevocationForEveryFrame` | Tested locally |
| Input validation | Bound frame size; reject bad magic, version, type, sequence and movement values. | Protocol unit and network integration tests | Tested; fuzz execution pending |
| Resource limits | Bound game connections, frame time, per-connection/per-IP movement rate and queued intents. | `NetworkServiceTest.RejectsConnectionsAboveTheConfiguredLimit`; `NetworkServiceTest.ClosesPartialFramesAfterTheReadDeadline`; `NetworkServiceTest.FloodRateLimitRejected`; `NetworkServiceIpRateTest.PerIpRateLimitSurvivesReconnect`; `NetworkServiceIpRateRecoveryTest.RecoversAfterPerIpFlood`; `IntentQueueTest.RejectsEnqueueWhenBoundIsReached` | Tested on loopback; the per-IP bucket does not limit handshakes and service capacity is unmeasured |
| State integrity | Store only validated intent and advance state on the server tick. | `NetworkServiceTest.AuthenticatedMovementUsesServerFixedTicks` | Tested locally |
| Cryptographic secret handling | Generate random credentials, store digests and clear the connection's credential buffer on close. | `SessionStore` implementation and lifecycle tests | Implemented; no independent cryptographic review |
| Error disclosure | Do not return protocol details on rejected connections. | Client receives a closed connection | Tested locally |
| Credential storage at rest | Protect demo TLS private keys and production credentials. | `setup-demo-secrets.ps1` restricts generated demo private keys to the current user. The server writes the reusable token as plaintext and restricts its file to the current user with a protected Windows DACL or POSIX mode 0600. The smoke test checks the file permissions. The container example uses tmpfs, but is not run. | Demo key and token file permission checks tested locally; production key controls remain residual risks |
| Logging and monitoring | Emit structured records without credentials and expose only implemented metrics. | `ServerSmokeTests.test_json_logs_parse_and_exclude_secrets`; `/metrics` currently exposes only `aegisguard_up 1` and has no operational counters. | Partial; no log retention or metrics instrumentation |
| Authorization | Require client certificates for read-only status and metrics routes. | `ServerSmokeTests.test_valid_token_admin_mtls_and_signal_shutdown` checks health, status and metrics authorization. There are no role-based or mutating admin actions. | Tested locally |
| Data retention | Define purpose, retention and deletion before collecting player behavior. | No persistent player telemetry exists | Deferred |
| Dependency and build security | Scan the built image and workflows; preserve SBOM/provenance. | The latest remote CI run passed Debug and Release build/test jobs and generated a source SBOM in Release. CodeQL result upload and Scorecard analysis failed. No container job or Trivy result appeared. | CodeQL and Scorecard **Not verified while the repository is private**; container scanning not verified |

Source: [OWASP ASVS 5.0.0](https://github.com/OWASP/ASVS/tree/v5.0.0), licensed
under CC BY-SA 4.0. This checklist paraphrases project-specific controls and
does not reproduce ASVS requirement text.
