# Test matrix

| Requirement | Verification | Latest evidence | Status |
|---|---|---|---|
| Invalid simulation input cannot partially advance state | Unit tests for invalid and duplicate movement | `SimulationTest.RejectsUnknownDuplicateAndInvalidInputsWithoutAdvancing` | Passing |
| Quadtree agrees with a simple spatial oracle | Seeded insert/update/remove/query sequence | `QuadtreeTest.MatchesBruteForceAcrossSeededOperationSequence` | Passing |
| Frame parser rejects malformed and out-of-range data | Protocol unit and fuzz-target tests | Negative unit cases pass; libFuzzer not run locally | Partial |
| TLS accepts only TLS 1.3 | Loopback client restricted to TLS 1.2 | `NetworkServiceTest.RejectsTlsTwelveClients` | Passed in 20/20 focused Debug repetitions and all 20 full Debug/Release suite runs after the MinGW Asio thread-model fix |
| Unknown credentials cannot submit movement | Loopback TLS authentication test | `NetworkServiceTest.RejectsUnknownCredentials` | Passing |
| Expiry and revocation apply during a connection | Session unit tests and live revocation test | `SessionStoreTest.ExpiresAndRevokesCredentials`; `NetworkServiceTest.RechecksCredentialRevocationForEveryFrame` | Passing |
| Incomplete input is bounded by a deadline | Send a full header and withhold its payload | `NetworkServiceTest.ClosesPartialFramesAfterTheReadDeadline` | Passing |
| Resource use is bounded for connections and movement | Connection-cap and token-bucket integration tests | `NetworkServiceTest.RejectsConnectionsAboveTheConfiguredLimit`; `NetworkServiceTest.AppliesThePerConnectionMovementRateLimit`; `NetworkServiceIpRateRecoveryTest.RecoversAfterPerIpFlood` | Passing; per-IP recovery tested |
| Accepted movement uses the server timestep | TLS client sends intent; test reads state on the server event loop | `NetworkServiceTest.AuthenticatedMovementUsesServerFixedTicks` | Passing |
| Session store supports concurrent validation and revocation | Multi-threaded store test | `SessionStoreTest.SupportsConcurrentValidationAndRevocation` | Passing; ThreadSanitizer unavailable |
| Protocol violation detection emits explainable test-only findings | Rule unit tests and authenticated TLS integration tests | `RuleDetectorTest` cases; movement-range, replay and rate-limit signals are wired through the network service | Passing for implemented protocol signals; gameplay false-positive evaluation is deferred |
| Replay validates recorded ticks and reproduces final state | Record accepted inputs, validate schema and tick order, replay from initial state | `ReplaySessionTest.RecordsAndReplaysValidatedSimulationInputs` | Passing; in-memory only, with bounded ticks and input records |
| Replay data has bounded retention and documented privacy limits | Configuration limits and data inventory review | [Replay documentation](../telemetry/replay.md) | Object-lifetime retention documented; no persistence, secure erasure or cryptographic integrity |
| Movement statistics are versioned and evaluated without gameplay claims | Feature tests and labeled synthetic evaluation traces | `MovementStatisticsTest`; `MovementStatisticsEvaluationTest` | Passing for offline synthetic examples only; real gameplay behavior is unmeasured |
| HTTPS routes enforce client-certificate authorization | Loopback tests for public health and protected status/metrics | `test_server_smoke.py` verifies health 200 and 401 without/200 with a trusted certificate for both status and metrics; an untrusted client certificate is rejected. | Passing for tested routes; ZAP is not run |
| Dependency, image and workflow risks are scanned | CodeQL, Scorecard, Trivy and CI checks | The latest CI run passed Debug and Release build/test jobs and succeeded at the Release SBOM step. CodeQL result upload and Scorecard analysis failed. No container job or Trivy result appeared. | CodeQL and Scorecard **Not verified while the repository is private**; Trivy not verified |
| Build provenance and SBOM are verifiable | Generate and verify release artifacts | The Release CI job generated a source SBOM. The tagged Cosign signing workflow has no recorded run. | Partial |
| Python benchmark-report sanitizer preserves data and removes host identifiers | Standard-library subprocess tests | `tests/python/test_sanitize_benchmark_json.py` | 2/2 passed locally |
