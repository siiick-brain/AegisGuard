# Architecture baseline

## Purpose and status

This document distinguishes implemented code from proposed components.
Milestones 0 to 2 are complete. A MinGW Asio thread-model mismatch that caused
intermittent test-process crashes was fixed and stress-verified in both Debug
and Release configurations. Milestone 4 added protocol-rule detection in
test-only policy mode. Milestone 5 added bounded in-memory replay sessions.
Persistent telemetry and deployment services have not started.

## Proposed trust boundaries and flow

```text
 Untrusted game client
   | input intent only; bounded, versioned framing implemented
   v
 [Network/session service: TLS 1.3 listener implemented]
   | accepted movement intent      | replay, rate and movement violations
   v                               v
 [Authoritative simulation]  [Rule detection: protocol violations implemented]
   |                               |
   v                               +----> [Evidence and telemetry boundary]
 [Spatial index]                             |
                                   bounded, versioned events
                                             v
                                  [Persistent evidence store: planned]

 Build and release inputs (source, dependencies, CI credentials) form a separate
 supply-chain trust boundary. No cloud or third-party service is required by
 this design baseline.
```

## Design principles

1. The server owns position, velocity, health, inventory, cooldowns, hit
   registration, and match outcomes. Client fields never overwrite these values.
2. Deterministic validation is on the authoritative path. Statistical or ML
   detector availability must not affect whether an action is valid.
3. Every untrusted-input path will be bounded in bytes, work, queueing, and
   lifetime. Exact limits must be measured and configured before networking ships.
4. Detection output is evidence with reasons and versioned features, not proof
   of intent. Enforcement is testable, reversible where possible, and policy
   driven.
5. Operational events and training datasets are separate. Collection is
   minimized, retention is explicit, and synthetic traces are labelled.
6. Optional infrastructure follows a working local system; diagrams and
   deployment descriptions must reflect what actually runs.

## Initial scope and deferred decisions

The simulation library owns player positions and velocities in a bounded 2D
world. Clients submit movement intent, and each accepted tick applies a fixed
time step. Invalid input rejects the whole tick without mutating player state.
GoogleTest covers configuration, input validation, movement, and world bounds.

The spatial module contains a dynamic point Quadtree with insertion, removal,
updates, inclusive range queries, and subtree merging after deletions. Queries
return sorted candidate IDs. They do not perform exact circle, box, or
game-specific collision tests. A seeded test sequence compares the result with
a brute-force oracle.

Google Benchmark workloads cover insertion, removal, updates, and range queries
for uniform and clustered synthetic points. The recorded report shows that
query cost depends on density and query size.

The network service terminates TLS 1.3, validates credentials against the
bounded session store, enforces per-connection sequences and rate limits, then
stores the latest movement intent for each connected player. The simulation
consumes these inputs on its fixed timestep. The service uses one Asio strand;
the simulation must not be accessed concurrently while the service is running.
The service and each connection share ownership of the `io_context` and TLS
context. Each asynchronous operation retains its connection until its handler
finishes. Callers must keep the session store and simulation alive until the
service has stopped and its `io_context::run()` thread has drained canceled
handlers. The test fixture follows this order and does not call `io_context::stop()`.
Tests exercise the flow over loopback TCP with generated test certificates.
Lifecycle events report when the service acquires and releases its shared TLS
context; callers create the context and may retain ownership beyond the
service.

An authenticated connection cancels its pending read before starting TLS
shutdown. The TLS shutdown handler closes the socket and releases the session.
Stopping during a handshake cancels the transport; it does not start TLS
shutdown before the handshake handler completes. Lifecycle events are written
to standard error with the `[aegisguard.tls]` prefix and contain no credentials.

The service defaults to loopback and has no production certificate provisioning,
credential issuance flow or client state replication. Its configured connection,
deadline and rate defaults are provisional, not load-tested capacity claims.

The rule detector observes authenticated movement-range violations, replayed
sequences and rate-limit violations. It emits versioned findings with reasons
and test-only escalating recommendations. It does not change simulation
validation or apply account-level penalties. See the
[rule detection notes](../detection/rule-detection.md).

`ReplaySession` is a caller-managed in-memory recorder for deterministic
simulation tests. It is not connected to the TLS service or persistent evidence
storage. Its data and retention limits are documented in
[replay documentation](../telemetry/replay.md).

TCP over TLS 1.3 is the proposed initial transport. The trade-off and remaining
work are recorded in [ADR 0002](decisions/0002-tcp-tls-transport.md). No custom
cryptographic primitive or ad-hoc secure UDP protocol is in scope.

Online ML inference, distributed event infrastructure, cloud deployment, and
operator UI are optional. They will be added only if measured value and a
documented operational need justify them.
