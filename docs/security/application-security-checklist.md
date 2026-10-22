# Application security checklist

This checklist adapts verification concerns from OWASP ASVS 5.0.0 to the
implemented game service. It is a development aid, not an ASVS assessment or
claim of compliance. ASVS is written for web applications and web services;
HTTP-specific items remain pending until an administrative API exists.

| Concern | Project check | Evidence | Status |
|---|---|---|---|
| Transport security | Require TLS 1.3; reject TLS 1.2-only clients. | `NetworkServiceTest.RejectsTlsTwelveClients` | Tested locally |
| Authentication | Validate a fixed-size credential and bind it to an existing server-owned player. | TLS integration tests and `SessionStoreTest` | Tested locally |
| Session lifecycle | Expire and revoke credentials; revalidate active sessions. | Session store tests and `NetworkServiceTest.RechecksCredentialRevocationForEveryFrame` | Tested locally |
| Input validation | Bound frame size; reject bad magic, version, type, sequence and movement values. | Protocol unit and network integration tests | Tested; fuzz execution pending |
| Resource limits | Bound active connections, frame time and movement rate. | Network integration tests | Tested with local defaults |
| State integrity | Store only validated intent and advance state on the server tick. | `NetworkServiceTest.AuthenticatedMovementUsesServerFixedTicks` | Tested locally |
| Cryptographic secret handling | Generate random credentials, store digests and clear the connection's credential buffer on close. | `SessionStore` implementation and lifecycle tests | Implemented; no independent cryptographic review |
| Error disclosure | Do not return protocol details on rejected connections. | Client receives a closed connection | Tested locally |
| Logging and monitoring | Avoid secret-bearing logs; record security counters without player IDs as metric labels. | Metrics and production logging are not implemented | Deferred |
| Authorization | Enforce administrative roles at the server boundary. | No administrative surface exists | Deferred |
| Data retention | Define purpose, retention and deletion before collecting player behavior. | No persistent player telemetry exists | Deferred |
| Dependency and build security | Scan dependencies and workflows; preserve SBOM/provenance. | CI security pipeline not implemented | Deferred |

Source: [OWASP ASVS 5.0.0](https://github.com/OWASP/ASVS/tree/v5.0.0), licensed
under CC BY-SA 4.0. This checklist paraphrases project-specific controls and
does not reproduce ASVS requirement text.
