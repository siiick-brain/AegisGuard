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

## Version 1 movement frame

The version 1 frame is exactly 18 bytes. Multi-byte integers use network byte
order.

| Offset | Length | Field | Accepted values |
|---:|---:|---|---|
| 0 | 2 | Magic | ASCII `AG` |
| 2 | 1 | Schema version | `1` |
| 3 | 1 | Message type | `1` for movement intent |
| 4 | 2 | Payload length | `4` |
| 6 | 8 | Sequence number | Unsigned, nonzero, strictly increasing per session |
| 14 | 2 | X direction | Signed integer from -1000 to 1000 |
| 16 | 2 | Y direction | Signed integer from -1000 to 1000 |

Direction values are divided by 1000 when decoded. The simulation applies its
own speed cap and world bounds. The frame carries no player ID or position.
It carries no score or authority to change game state. The connection/session
layer must bind an authenticated session to a server-owned player before
dispatching intent.

The parser rejects incomplete or oversized payloads and extra trailing bytes.
It rejects unknown versions or message types. It also rejects zero sequences
and out-of-range movement. The frame size is fixed. The decoder allocates no
memory based on a client
length field. The listener validates the 14-byte header before reading the
fixed four-byte movement payload. One absolute frame deadline covers both
reads.

## Connection flow

1. The client completes a TLS handshake. The service context allows TLS 1.3
   only. Compression and renegotiation are disabled.
2. The client sends one 32-byte session credential inside the encrypted
   connection. Credentials must be issued through a trusted server-side path
   that is not part of this repository.
3. The service validates the credential and confirms that its player already
   exists in the simulation. Each player may have only one active connection.
4. The client sends version 1 movement frames. The service revalidates the
   credential for every frame and every simulation tick. Expiry or revocation
   closes the connection and removes its movement intent.
5. The service stores only the latest movement intent per connected player.
   The map is bounded by the connection cap. The simulation consumes these
   inputs on its fixed timestep rather than once per client packet.

Authentication and frame reads have separate deadlines. The TLS/authentication
deadline is three seconds. A complete frame has a one-second deadline. The default
connection cap is 128. Each connection has a token bucket with capacity 16 and
refill rate 60 frames per second. Evaluate these initial values under workload
before deployment.

## Sequence and rate controls

`SequenceGuard` accepts strictly increasing values and rejects duplicates or
out-of-order input. It is scoped to one authenticated connection. `TokenBucket`
uses a monotonic clock and rejects clock regression. Invalid framing and
authentication close the connection without returning protocol detail to the
client. Replay and rate-limit events do the same.

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
