# Initial STRIDE threat model

## Status and scope

This threat model covers the local authoritative game server and its initial
TLS listener. The browser dashboard and cloud deployment are out of scope.
Third-party network edges are out of scope too because they are not implemented.

## Assets

- Authoritative match state and outcomes.
- Session credentials and session lifecycle plus any future signing keys.
- Availability and bounded resource consumption of the server.
- Detection evidence and replay data plus any pseudonymous player references.
- Build/release integrity and dependency provenance plus CI credentials.

## Trust boundaries

1. **Client to server:** client bytes and timing cross a TLS 1.3 listener. The
   service validates session credentials and framing. It checks sequence and
   rate before accepting movement intent.
2. **Simulation to detector:** protocol findings can inform review. The
   test-only recommendations do not change accepted simulation state or apply
   account penalties. Findings are delivered through a caller callback.
3. **Server to evidence store:** emitted events may contain sensitive behavioral
   data. The store requires schema validation and bounded writes. It also
   requires access control and retention plus integrity measures.
4. **Source/dependency/CI to release:** repository changes and dependencies
   can affect distributed artifacts. So can workflow inputs and release
   credentials.

## STRIDE analysis

| Threat | Example risk | Planned controls and evidence |
|---|---|---|
| Spoofing | A caller impersonates another player or reuses an expired session. | TLS 1.3 protects transport. A bounded session store issues random credentials, binds them to server-supplied player IDs, expires them and supports revocation. The listener revalidates credentials for every frame and tick. Credential issuance and player provisioning remain outside the service. |
| Tampering | A client submits a position, inventory, score, or hit result as trusted state. | Accept intent only; derive state and outcomes on the server; test state invariants and reject unauthorized fields. |
| Repudiation | An enforcement action cannot be explained or reconstructed. | Protocol findings include a version, signal, reason and count and reach a caller callback. Persistent evidence, replay, access controls and account-level enforcement remain unimplemented. |
| Information disclosure | Logs or exported evidence leak credentials or unnecessary personal data. | Never log secrets; minimize and pseudonymize fields; separate datasets; define access, retention, and deletion before collecting real telemetry. |
| Denial of service | Oversized messages, floods, slow handshakes or unbounded connections exhaust resources. | Fixed-size frames, a 128-connection default cap, authentication and frame deadlines, per-connection rate limits, bounded latest-intent storage and bounded TLS shutdown. Defaults need workload testing. |
| Elevation of privilege | A player invokes admin actions or CI/deployment credentials are over-scoped. | Explicit authorization roles, least-privilege workflows and service accounts, protected secrets, and negative authorization tests. Operator/admin surfaces are not yet implemented. |

## Initial attack scenarios

- Forged or impossible movement is submitted as an authoritative state update.
- Duplicate or reordered actions are used to repeat resource-consuming operations.
- A flood of malformed requests consumes parser or simulation capacity.
- A weak behavioral signal causes an unjustified penalty against a legitimate
  player. This could include someone using an accessibility tool or a
  high-latency link.
- Evidence is modified or retained longer than its documented purpose.
- A compromised dependency or CI credential alters a release artifact.

## Residual risks and open questions

- The TLS 1.3 listener is tested locally with generated certificates. Production
  certificate provisioning and key rotation remain open. Session issuance and
  workload-tuned rate limits remain undefined.
- Server-side checks cannot prove a client is honest. Legitimate latency and
  accessibility needs can resemble suspicious behavior. So can unusual play.
- A local hash chain alone cannot prevent a sufficiently privileged attacker
  from rewriting the full evidence store.
- Dense spatial queries can cost more than a linear scan. The checked-in
  synthetic benchmark demonstrates this for a query that covers a dense cluster;
  production entity caps and query budgets have not been defined.
- Synthetic adversarial traces do not establish coverage of real cheats or
  real-world false-positive rates.
- The libFuzzer harness exists but was not run because Clang is unavailable in
  the current Windows toolchain. Load validation remains incomplete. Privacy
  and supply-chain validation remain unperformed.

Review and update this model at the protocol and telemetry milestones. Review
it at CI and deployment milestones too. Update it when new assets or trust
boundaries are introduced.
