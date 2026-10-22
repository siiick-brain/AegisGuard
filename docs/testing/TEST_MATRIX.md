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
| Resource use is bounded for connections and movement | Connection-cap and token-bucket integration tests | `NetworkServiceTest.RejectsConnectionsAboveTheConfiguredLimit`; `NetworkServiceTest.AppliesThePerConnectionMovementRateLimit` | Passing |
| Accepted movement uses the server timestep | TLS client sends intent; test reads state on the server event loop | `NetworkServiceTest.AuthenticatedMovementUsesServerFixedTicks` | Passing |
| Session store supports concurrent validation and revocation | Multi-threaded store test | `SessionStoreTest.SupportsConcurrentValidationAndRevocation` | Passing; ThreadSanitizer unavailable |
| Protocol violation detection emits explainable test-only findings | Rule unit tests and authenticated TLS integration tests | `RuleDetectorTest` cases; movement-range, replay and rate-limit signals are wired through the network service | Passing for implemented protocol signals; gameplay false-positive evaluation is deferred |
| Replay validates recorded ticks and reproduces final state | Record accepted inputs, validate schema and tick order, replay from initial state | `ReplaySessionTest.RecordsAndReplaysValidatedSimulationInputs` | Passing; in-memory only, with bounded ticks and input records |
| Replay data has bounded retention and documented privacy limits | Configuration limits and data inventory review | [Replay documentation](../telemetry/replay.md) | Object-lifetime retention documented; no persistence, secure erasure or cryptographic integrity |
| Movement statistics are versioned and evaluated without gameplay claims | Feature tests and labeled synthetic evaluation traces | `MovementStatisticsTest`; `MovementStatisticsEvaluationTest` | Passing for offline synthetic examples only; real gameplay behavior is unmeasured |
| HTTP API role checks resist direct requests | Direct authorization tests plus ZAP/Schemathesis | No HTTP API exists | Planned |
| Dependency, image and workflow risks are scanned | CodeQL, Scorecard, Trivy, Dependency-Check and CI checks | CodeQL and Scorecard workflows are configured but have not run; Trivy/Dependency-Check are deferred due to absent lockfile/container/IaC targets | Partial |
| Build provenance and SBOM are verifiable | Generate and verify release artifacts | Syft-based source SBOM generation is configured; no release binary or attestation exists | Partial |
| Python benchmark-report sanitizer preserves data and removes host identifiers | Standard-library subprocess tests | `tests/python/test_sanitize_benchmark_json.py` | 2/2 passed locally |
