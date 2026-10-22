# Test matrix

| Requirement | Verification | Latest evidence | Status |
|---|---|---|---|
| Invalid simulation input cannot partially advance state | Unit tests for invalid and duplicate movement | `SimulationTest.RejectsUnknownDuplicateAndInvalidInputsWithoutAdvancing` | Passing |
| Quadtree agrees with a simple spatial oracle | Seeded insert/update/remove/query sequence | `QuadtreeTest.MatchesBruteForceAcrossSeededOperationSequence` | Passing |
| Frame parser rejects malformed and out-of-range data | Protocol unit and fuzz-target tests | Negative unit cases pass; libFuzzer not run locally | Partial |
| TLS accepts only TLS 1.3 | Loopback client restricted to TLS 1.2 | `NetworkServiceTest.RejectsTlsTwelveClients` | Passing |
| Unknown credentials cannot submit movement | Loopback TLS authentication test | `NetworkServiceTest.RejectsUnknownCredentials` | Passing |
| Expiry and revocation apply during a connection | Session unit tests and live revocation test | `SessionStoreTest.ExpiresAndRevokesCredentials`; `NetworkServiceTest.RechecksCredentialRevocationForEveryFrame` | Passing |
| Incomplete input is bounded by a deadline | Send a full header and withhold its payload | `NetworkServiceTest.ClosesPartialFramesAfterTheReadDeadline` | Passing |
| Resource use is bounded for connections and movement | Connection-cap and token-bucket integration tests | `NetworkServiceTest.RejectsConnectionsAboveTheConfiguredLimit`; `NetworkServiceTest.AppliesThePerConnectionMovementRateLimit` | Passing |
| Accepted movement uses the server timestep | TLS client sends intent; test reads state on the server event loop | `NetworkServiceTest.AuthenticatedMovementUsesServerFixedTicks` | Passing |
| Session store supports concurrent validation and revocation | Multi-threaded store test | `SessionStoreTest.SupportsConcurrentValidationAndRevocation` | Passing; ThreadSanitizer unavailable |
| Detection avoids false positives on legitimate traces | Labeled synthetic evaluation and threshold analysis | No detector or corpus exists | Planned |
| HTTP API role checks resist direct requests | Direct authorization tests plus ZAP/Schemathesis | No HTTP API exists | Planned |
| Dependency, image and workflow risks are scanned | Trivy, Dependency-Check and CI checks | Tools and CI integration are not present | Planned |
| Build provenance and SBOM are verifiable | Generate and verify release artifacts | No release pipeline exists | Planned |
