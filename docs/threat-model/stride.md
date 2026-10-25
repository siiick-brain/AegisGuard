# STRIDE threat model

## Status and scope

This model describes the current development server, not a production game
deployment. The service has a TLS 1.3 binary game listener and a separate HTTPS
health/status listener. There is no game client, login system, persistent
evidence database or complete match lifecycle.

The game listener uses TLS and a bearer session credential; it does not require
a client certificate. The HTTPS listener leaves `/healthz` public and requires
a trusted client certificate for `/status` and `/metrics`. The default server
bind is loopback. The container example binds inside the container and maps
ports to host loopback; its Linux CI job has not run. Cloudflare is **Not
deployed**. Its future HTTP WAF/rate limits would not protect the custom game
protocol on TCP port 18443.

This model distinguishes implemented controls from configured but unverified
deployment controls and from planned work. The latest remote CI run passed
Debug and Release build/test jobs and generated a source SBOM in Release.
CodeQL result upload and Scorecard analysis failed. No container job appeared.
CodeQL and Scorecard are **Not verified while the repository is private**.
Test evidence is local and synthetic unless stated otherwise. It is not an
independent security review.

## Assets and data

- Simulation state and server-side player IDs: in memory.
- Session credential digests: in-memory `SessionStore`; the standalone demo
  writes the reusable plaintext credential as hex to its configured token
  file at startup. The file is restricted to the current user with a protected
  Windows DACL or POSIX mode 0600. The
  container example places the token in a mode-0700 tmpfs directory, but that
  image has not yet been run.
- Client movement, aim and position-claim telemetry: processed in memory.
  Claimed positions are not authoritative simulation state.
- Rule findings: emitted to the configured callback and logged by the demo;
  there is no persistent evidence store or account enforcement.
- Replay sessions: supported by an in-memory library; they are not connected
  to network request processing or persisted by the standalone server.
- JSON Lines process logs: written to standard error. The application does
  not provide log retention, access control or tamper evidence.
- TLS private keys and CA files: the demo setup script creates disposable
  certificates and restricts private keys to the current user. Production
  key provisioning, rotation and protection are outside the repository.
- Source, CI workflows and the container build: CI is configured with
  read-only repository permissions and pinned actions. The latest remote CI
  run verified only its Debug and Release jobs; container execution and
  release provenance are not verified.

## Trust boundaries

1. **Game client to TCP listener.** TLS 1.3 protects transport. The client
   supplies a reusable 32-byte session credential and protocol frames. There
   is no identity provider or game-client certificate.
2. **Listener to simulation and detector.** Only normalized direction intent
   is queued for fixed-tick simulation. Client-reported positions and aim are
   untrusted anomaly telemetry. Rule recommendations are observe-only; active
   protocol and movement gates still reject frames and close connections.
3. **HTTPS client to status listener.** Health is public. Status and metrics
   require a certificate trusted by the configured admin CA. The listener has
   no mutating administrative operations. It has an 8 KiB request buffer and
   a three-second connection/request timer, but no explicit connection-count
   cap or per-IP HTTP rate limit.
4. **Process to local files and logs.** The demo writes a plaintext token file
   with owner-only permissions and writes JSON Lines to standard error. Log
   collection and retention depend on the host or container runtime.
5. **Source and CI to a possible image or release.** The Dockerfile, Compose
   hardening and CI build/scan job are configured. They have not been verified
   by a Docker/Trivy run in this work session. No image is published or signed.
6. **Cloudflare edge to origin.** No such boundary exists yet. No domain,
   Tunnel, DNS record, WAF rule or rate-limit rule is deployed. The game TCP
   protocol is outside HTTP WAF protection.

## STRIDE analysis

| Threat | Current assessment and actual control |
|---|---|
| Spoofing | A bearer credential is validated against an in-memory digest bound to a server-supplied player ID and rechecked during a connection. TLS 1.3 protects it in transit. The demo issues a token at startup and writes it in plaintext with owner-only file permissions; there is no login, client certificate on the game listener or token rotation. Credential theft remains a risk. |
| Tampering | Versioned framing, sequence checks and simulation bounds reject malformed or replayed input. The simulation applies direction intent at its fixed tick; client position claims are not copied into authoritative state. An authorized client can still submit legal but adversarial input and heuristic telemetry can be falsified. |
| Repudiation | JSON Lines and versioned rule findings provide limited process evidence. Findings are callback-driven and recommendations remain observe-only. Logs are not durable or tamper-evident. The replay library is in-memory and is not wired into live request processing. There is no audit guarantee. |
| Information disclosure | The JSON logger escapes values and the smoke test checks emitted records for secret-named fields and known token/key values. This is not a general proof that future log messages are safe. The demo token file is plaintext; the application applies a protected current-user DACL on Windows or mode 0600 on POSIX, and the smoke test checks the resulting permissions. Host administrators can still access the file. No persistent player telemetry store exists. |
| Denial of service | The game listener bounds connections, frame sizes, authentication/frame time and movement queues. Per-connection and shared per-IP token buckets apply to authenticated movement frames; the IP bucket does not limit TLS handshakes or unauthenticated connection attempts. The HTTPS listener has a bounded request buffer and timer but no explicit concurrent-connection or per-IP cap. These controls do not provide DDoS protection or prove capacity. |
| Elevation of privilege | `/status` and `/metrics` are read-only and require a trusted client certificate; `/healthz` is intentionally public. No role-based administrative actions exist. Container privilege reduction is configured but not yet executed. CI and deployment credentials are not stored in this repository. |

## Attack trees

### Alter authoritative player state

```text
Goal: make the server accept an unauthorized or impossible position
  OR
  +-- Submit a client position claim as state
  |     `-- Claim is telemetry only; simulation consumes direction intent
  +-- Bypass protocol framing or version checks
  |     +-- Send old protocol version -> rejected
  |     +-- Omit HELLO or send malformed/oversized frame -> rejected
  |     `-- Replay a sequence -> connection closed
  +-- Submit valid intent with excessive movement telemetry
  |     +-- Speed, teleport distance and aim snap gates reject tested cases
  |     `-- Latency, jitter and fast-turn examples are tolerated synthetically
  `-- Steal a valid bearer credential
        `-- TLS protects transit; the plaintext demo token file remains
            accessible to the current user and production credential rotation
            is absent
```

The movement gates are heuristics, not proof of cheating. Reported position
and aim values are client-controlled, so an adversary may choose plausible
telemetry to evade those checks. This cannot directly set authoritative
position. Synthetic false-positive cases do not establish real-player rates.

### Exhaust server resources

```text
Goal: reduce game or status service availability
  OR
  +-- Open many game connections or hold authentication
  |     `-- Connection cap and authentication deadline bound accepted work
  |         but there is no pre-authentication per-IP connection limiter
  +-- Send oversized, malformed or incomplete frames
  |     `-- Fixed frame bounds, parser rejection and read deadline
  +-- Flood valid movement frames
  |     +-- Per-connection token bucket
  |     +-- Shared per-IP token bucket for authenticated movement
  |     `-- Bounded per-player FIFO rejects a full queue
  +-- Consume many IP-rate state entries
  |     `-- Map capacity is bounded and idle entries expire in memory
  `-- Open many HTTPS connections
        `-- Request size and per-connection time are bounded; no explicit
            concurrent-connection or per-IP HTTP rate cap is implemented
```

The per-IP bucket state is in-process memory, shared across reconnects for up
to five idle minutes. It is not durable across server restarts. Clients behind
one NAT share a bucket. No network-edge DDoS mitigation is deployed.

### Read protected status or metrics

```text
Goal: retrieve protected operator data
  OR
  +-- Request /status without a trusted client certificate -> 401
  +-- Request /metrics without a trusted client certificate -> 401
  +-- Request /healthz -> public status only, intentionally available
  `-- Bypass or misuse a Cloudflare edge
        `-- No Cloudflare edge exists; no WAF protection is claimed
```

The application checks its configured client CA at the origin. A future Tunnel
must preserve this check; Cloudflare edge client authentication must not be
assumed to authenticate the separate origin connection.

## Control-to-test map

| Implemented control | Named verification | Limit or status |
|---|---|---|
| TLS 1.3 only and credential validation | `NetworkServiceTest.RejectsTlsTwelveClients`; `NetworkServiceTest.RejectsUnknownCredentials`; `SessionStoreTest.IssuesRandomCredentialsBoundToOnePlayer` | Loopback test certificates; not production PKI |
| Expiry, revocation and per-frame revalidation | `SessionStoreTest.ExpiresAndRevokesCredentials`; `NetworkServiceTest.RechecksCredentialRevocationForEveryFrame`; `NetworkServiceTest.StopsMovementWhenAnIdleSessionExpires` | In-memory lifecycle only |
| Versioned HELLO, protocol-version and frame validation | `ProtocolTest.RequiresExplicitVersionedHello`; `ProtocolTest.RejectsOldProtocolVersion`; `NetworkServiceTest.RejectsOldProtocolVersion`; `NetworkServiceTest.RejectsMovementBeforeHello`; `ProtocolTest.RejectsMalformedHeadersAndLengths`; `ProtocolTest.RejectsIncompleteFramesTrailingBytesAndInvalidSequence` | No downgrade negotiation |
| Bounded frame reads and deadlines | `NetworkServiceTest.RejectsOversizedFrameOnLoopback`; `NetworkServiceTest.ClosesPartialFramesAfterTheReadDeadline`; `NetworkServiceTest.ClosesUnauthenticatedConnectionsAfterTheDeadline` | Loopback tests; no capacity claim |
| Loopback default and bounded rate/queue configuration | `ServerConfigTest.DefaultsToLoopback`; `ServerConfigTest.ParsesQueueAndPerIpLimits`; `ServerConfigTest.RejectsInvalidDocumentAndPort` | The container's host-port binding is separately configured but not runtime-tested |
| Server-authoritative movement and untrusted position claims | `NetworkServiceTest.AuthenticatedMovementUsesServerFixedTicks`; `NetworkServiceTest.ForgedPositionRejected`; `SimulationTest.CapsDiagonalMovementToMaximumSpeed`; `SimulationTest.ClampsMovementToWorldBounds` | Client controls remain adversarial |
| Speed, teleport and aim anomaly gates | `MovementAnomalyTest.SpeedCheatRejected`; `NetworkServiceTest.SpeedCheatRejected`; `MovementAnomalyTest.TeleportCheatRejected`; `NetworkServiceTest.TeleportCheatRejected`; `MovementAnomalyTest.AimSnapCheatRejected`; `NetworkServiceTest.AimSnapCheatRejected` | Thresholds are synthetic defaults |
| Latency, jitter and legitimate-turn tolerance | `MovementAnomalyTest.FalsePositiveLatency`; `MovementAnomalyTest.FalsePositiveJitter`; `MovementAnomalyTest.FalsePositiveFastLegitimateTurn` | Not calibrated on real players or networks |
| Replay and movement-rate controls | `ProtocolTest.AcceptsOnlyStrictlyIncreasingSequences`; `ProtocolTest.EnforcesTokenBucketCapacityAndRefill`; `NetworkServiceTest.ClosesConnectionsWhenASequenceIsReplayed`; `NetworkServiceTest.FloodRateLimitRejected` | Movement rate tests do not measure throughput |
| Connection caps and shared IP-rate state | `NetworkServiceTest.RejectsConnectionsAboveTheConfiguredLimit`; `IpRateLimiterTest.StateSurvivesReconnectsFromTheSameAddress`; `IpRateLimiterTest.TracksAddressesIndependentlyAndBoundsState`; `NetworkServiceIpRateTest.PerIpRateLimitSurvivesReconnect`; `NetworkServiceIpRateRecoveryTest.RecoversAfterPerIpFlood` | IP rate check applies to authenticated movement, not handshakes |
| Bounded FIFO ordering, hold-last behavior and capacity | `IntentQueueTest.PreservesFifoOrderAndHoldsLastIntent`; `IntentQueueTest.RejectsEnqueueWhenBoundIsReached` | The NetworkService queue-full close/report path lacks a dedicated integration test; no service capacity claim |
| Observe-only, explainable rule findings | `RuleDetectorTest.EmitsVersionedEvidenceAndKeepsEnforcementInTestMode`; `RuleDetectorTest.DescribesEachSupportedSignal`; `NetworkServiceTest.ReportsOutOfRangeMovementAsTestOnlyEvidence` | No automatic ban, account action or persistent finding store |
| JSON Lines format and sensitive-field checks | `ServerSmokeTests.test_json_logs_parse_and_exclude_secrets`; `ServerSmokeTests.test_valid_token_admin_mtls_and_signal_shutdown` | One logger probe and one local smoke run; future messages need review |
| HTTPS health and protected read-only routes | `ServerSmokeTests.test_valid_token_admin_mtls_and_signal_shutdown` | Tests health 200 and status/metrics 401 without and 200 with a trusted client certificate. No ZAP run. |
| HTTPS request-buffer and timeout bounds | No direct test currently exercises the 8 KiB request limit or three-second admin timer | Implemented in the listener; validation gap remains |
| Container identity, filesystem and Linux restrictions | CI `container` job: Compose build/run, inspect assertions, `/healthz` curl and Trivy scan | Configured but not run; do not claim a passing image or scan |

## Controls not present

- No game-client mTLS, account login, external identity provider or automated
  player provisioning.
- No production credential/key rotation workflow or production certificate
  provisioning.
- No persistent audit log, replay recording from live traffic, tamper-evident
  store, real-player data, retention/deletion workflow or account-level
  enforcement.
- No game-protocol WAF, Cloudflare deployment, public ingress, network-layer
  DDoS mitigation or HTTP connection/rate limiter.
- No verified Docker run, Trivy result, signed release image or build
  provenance. The container CI job is configured but unexecuted.
- No dedicated integration test for HTTPS request-buffer/timeout rejection or
  the NetworkService queue-full close/report path.
- No calibrated real-game cheat detection, real-world false-positive rate or
  service capacity/soak result.

Revisit this model after the first Linux container CI run, after Cloudflare
inputs are provided and before any public deployment or release.
