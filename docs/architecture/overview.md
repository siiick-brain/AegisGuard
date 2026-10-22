# Architecture baseline

## Purpose and status

This document distinguishes implemented code from proposed components.
Milestones 0 to 2 are complete. The TLS service implementation is in place,
but Milestone 3 validation remains open while a one-off Debug process crash is
investigated. Detection, telemetry and deployment services have not started.

## Proposed trust boundaries and flow

```text
 Untrusted game client
   | input intent only; bounded, versioned framing implemented
   v
 [Network/session service: TLS 1.3 listener implemented]
   | authenticated, rate-limited, bounded movement frames
   v
 [Authoritative simulation: fixed-step movement implemented]
   | validate action -> advance fixed-step state -> determine outcome
   +------------------------------+
   |                              |
   v                              v
 [Spatial index]          [Rule/statistical detection: planned]
   |                              |
   +-------------> [Evidence and telemetry boundary]
                              |
                    bounded, versioned events
                              v
                 [Local store/replay: planned]

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
Tests exercise this flow over loopback TCP with generated test certificates.

The service defaults to loopback and has no production certificate provisioning,
credential issuance flow or client state replication. Its configured connection,
deadline and rate defaults are provisional, not load-tested capacity claims.

TCP over TLS 1.3 is the proposed initial transport. The trade-off and remaining
work are recorded in [ADR 0002](decisions/0002-tcp-tls-transport.md). No custom
cryptographic primitive or ad-hoc secure UDP protocol is in scope.

Online ML inference, distributed event infrastructure, cloud deployment, and
operator UI are optional. They will be added only if measured value and a
documented operational need justify them.
