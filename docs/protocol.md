# Protocol and network service

## Status

The repository contains a TLS 1.3 TCP listener with session authentication and
bounded frame parsing. It also has replay protection and per-connection rate
limiting with fixed-tick simulation dispatch. Integration tests use loopback
sockets and an ephemeral test certificate. This is a development service and
not a production deployment. It defaults to loopback. Certificate provisioning
and key rotation are not implemented. Neither are player provisioning or
production monitoring.

All listener callbacks and session validation run through one Asio strand.
Simulation ticks use the same strand. `SessionStore` protects its data with a
mutex. Callers must not access `Simulation` concurrently with a running
service. The referenced `io_context` and TLS context must remain alive.
The session store and simulation must remain alive as well. The error callback
must remain alive until service shutdown has completed.

## Version 3 protocol

After the TLS handshake and 32-byte session credential, the client must send
one versioned HELLO before game intents. Every message uses a 14-byte common
header. Multi-byte integers use network byte order. Version 1 and version 2
clients are rejected; there is no downgrade or implicit version negotiation.

| Offset | Length | Field | Accepted values |
|---:|---:|---|---|
| 0 | 2 | Magic | ASCII `AG` |
| 2 | 1 | Schema version | `3` |
| 3 | 1 | Message type | `0` HELLO; `1` movement; `2` FIRE intent |
| 4 | 2 | Payload length | `2` for HELLO; `14` for movement; `10` for FIRE |
| 6 | 8 | Sequence number | Zero for HELLO; nonzero and strictly increasing for movement and FIRE |

The HELLO payload is one unsigned 16-bit protocol version. It must equal `3`.
Its frame is exactly 16 bytes.

The movement payload is exactly 14 bytes:

| Payload offset | Length | Field | Accepted values |
|---:|---:|---|---|
| 0 | 2 | X direction | Signed integer from -1000 to 1000 |
| 2 | 2 | Y direction | Signed integer from -1000 to 1000 |
| 4 | 2 | Aim angle | Unsigned turn fraction mapped to `[0, 2π)` |
| 6 | 4 | Reported X position | Signed integer in thousandths of a world unit |
| 10 | 4 | Reported Y position | Signed integer in thousandths of a world unit |

The complete movement frame is exactly 28 bytes. Direction values are divided
by 1000 when decoded. The position fields are untrusted client telemetry used
only by speed and teleport anomaly checks. They are never copied to player
state. The simulation continues to use the direction vector, its own speed
cap and world bounds. The frame carries no player ID, score or authority to
change game state. The authenticated session binds the connection to a
server-owned player before intent is dispatched.

The FIRE payload is exactly 10 bytes:

| Payload offset | Length | Field | Accepted values |
|---:|---:|---|---|
| 0 | 2 | Aim angle | Unsigned turn fraction mapped to `[0, 2π)` |
| 2 | 8 | Tick reference | Server simulation tick; must not be future-dated or older than the rewind window |

The complete FIRE frame is exactly 24 bytes. It carries no target ID, health
value or claimed hit result. The authenticated session supplies the shooter
identity. The server checks the referenced position snapshot, queries the
Quadtree for candidate targets and performs an exact ray-to-target test. The
server applies damage and schedules respawn; no client field can set health or
declare a hit.

The parser rejects incomplete or oversized payloads and extra trailing bytes.
It rejects unknown versions or message types, unsupported HELLO values, zero
game-intent sequences, out-of-range direction and FIRE payloads containing
extra claim fields. The maximum frame buffer is fixed at 28 bytes. The decoder
allocates no memory based on a client length field. The listener validates the
14-byte header before reading the bounded payload. A frame deadline covers the
header and payload reads.

## Connection flow

1. The client completes a TLS handshake. The service context allows TLS 1.3
   only. Compression and renegotiation are disabled.
2. The client sends one 32-byte session credential inside the encrypted
   connection. Credentials must be issued through a trusted server-side path
   that is not part of this repository.
3. The service validates the credential and confirms that its player already
   exists in the simulation. Each player may have only one active connection.
4. The client sends a version 3 HELLO followed by version 3 movement or FIRE
   intents. The service revalidates the credential for every frame and every
   simulation tick. Expiry or revocation closes the connection and removes
   its movement intent.
5. The service stores movement intents in a bounded FIFO for each connected
   player. The default queue holds 64 pending intents. Each simulation tick
   consumes one queued value and holds the last applied intent until the next
   value arrives. A full queue rejects the frame and closes the connection.
   Disconnecting removes that player's queue.

Authentication and frame reads have separate deadlines. The TLS/authentication
deadline is three seconds. A complete frame has a one-second deadline. The default
connection cap is 128. Each connection has a token bucket with capacity 16 and
refill rate 60 frames per second. The shared per-IP bucket defaults to
capacity 128 and refill rate 120 frames per second. It tracks at most 4096
addresses and retains idle state for five minutes; reconnects from an address
reuse its existing bucket during that period. The per-player queue defaults
to 64 entries and is capped at 1024. Evaluate these initial values under
workload before deployment.

## Server-owned combat state

Players begin with 100 health. A valid FIRE intent applies 25 damage to the
nearest eligible player along the aim ray within 20 world units. The default
target radius is 0.5 world units. FIRE uses one of the most recent seven
server snapshots, including the current tick; references in the future or
older than six ticks are rejected. Eliminated players stop moving and respawn
with full health at their configured server-owned spawn position after 180
simulation ticks. These values are prototype defaults, not gameplay balance
results.

## Sequence and rate controls

`SequenceGuard` accepts strictly increasing values and rejects duplicates or
out-of-order input across movement and FIRE frames. It is scoped to one
authenticated connection. `TokenBucket` uses a monotonic clock and rejects
clock regression. Invalid framing and authentication close the connection
without returning protocol detail to the client. Replay and rate-limit events
do the same.

## Movement anomaly gates

Movement frames include client-reported position and aim telemetry. The
server compares position displacement against configured simulation speed
with a 250 ms latency grace and a 1.0-world-unit jitter allowance. A
displacement above 10 world units is classified as a teleport before the
speed check. Aim changes are compared using the shortest wrapped angular
distance; the default bound is 40 radians per second with a 20 ms grace.
These thresholds are engineering defaults, not empirical gameplay
calibration. Anomalous frames are rejected and the connection is closed.

The tests include deterministic latency, jitter and fast-turn examples. They
do not establish false-positive rates on real networks or validate a real
player population.

## Session credentials

`SessionStore` creates 256-bit credentials with OpenSSL's secure random
generator. It stores a SHA-256 digest rather than the reusable credential.
Each entry is bound to a server-supplied player ID. Credential lifetime ranges
from one second through one hour. The store supports revocation and caps the
number of stored sessions. Its default capacity is 4096. Expired entries are
removed during validation or before another credential is issued. Public operations are
mutex-protected.

The caller receives the plaintext credential once and must protect it.
`SessionStore` is mutex-protected. The network service uses it to validate
credentials during authentication and before each frame and simulation tick.
It does not implement a login flow or establish identity independently of the
server-supplied player ID.

## Transport decision and remaining work

The service uses TCP over TLS 1.3. Reliable ordered delivery fits the current
low-rate input protocol. It avoids a custom UDP reliability or congestion
layer. TCP head-of-line blocking may affect gameplay latency. Measure it before
considering another transport. Asio 1.36.0 uses Boost Software License 1.0.
OpenSSL 3.6.1 uses Apache License 2.0.

The server stops accepting and ticking before closing active connections. It
closes unauthenticated sockets immediately. It attempts TLS shutdown for
authenticated connections with a 250 ms deadline. The service sends no state
snapshots or acknowledgements to clients. The tests do not measure overload
capacity or connection churn. They do not measure latency percentiles or
long-run resource use.
