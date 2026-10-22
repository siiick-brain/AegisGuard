# Protocol and network service

## Status

The repository contains a TLS 1.3 TCP listener, session authentication, bounded
frame parsing, replay protection, per-connection rate limiting and fixed-tick
simulation dispatch. Integration tests use loopback sockets and an ephemeral
test certificate. This is a development service, not a production deployment.
It defaults to loopback. Certificate provisioning, key rotation, player
provisioning and production monitoring are not implemented.

All listener callbacks, session validation and simulation ticks run through one
Asio strand. `SessionStore` protects its own data with a mutex. Callers must not
access `Simulation` concurrently with a running service. The referenced
`io_context`, TLS context, session store, simulation and error callback must
remain alive until service shutdown has completed.

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
own speed cap and world bounds. The frame carries no player ID, position, score,
or authority to change game state. The connection/session layer must bind an
authenticated session to a server-owned player before dispatching intent.

The parser rejects incomplete or oversized payloads, extra trailing bytes,
unknown versions or message types, zero sequences and out-of-range movement.
The frame size is fixed, so the decoder allocates no memory based on a client
length field. The listener validates the 14-byte header before reading the
fixed four-byte movement payload. One absolute frame deadline covers both
reads.

## Connection flow

1. The client completes a TLS handshake. The service context is restricted to
   TLS 1.3, with compression and renegotiation disabled.
2. The client sends one 32-byte session credential inside the encrypted
   connection. Credentials must be issued through a trusted server-side path
   that is not part of this repository.
3. The service validates the credential and confirms that its player already
   exists in the simulation. Only one active connection is allowed per player.
4. The client sends version 1 movement frames. The service revalidates the
   credential for every frame and every simulation tick. Expiry or revocation
   closes the connection and removes its movement intent.
5. The service stores only the latest movement intent per connected player.
   The map is bounded by the connection cap. The simulation consumes these
   inputs on its fixed timestep, not once per client packet.

Authentication and frame reads have separate deadlines. Defaults are three
seconds for TLS/authentication and one second for a complete frame. The default
connection cap is 128. Each connection has a token bucket with capacity 16 and
refill rate 60 frames per second. These initial values need workload evaluation
before deployment.

## Sequence and rate controls

`SequenceGuard` accepts strictly increasing values and rejects duplicates or
out-of-order input. It is scoped to one authenticated connection. `TokenBucket`
uses a monotonic clock and rejects clock regression. Invalid framing,
authentication, replay and rate-limit events close the connection without
returning protocol detail to the client.

## Session credentials

`SessionStore` creates 256-bit credentials with OpenSSL's secure random
generator. It stores a SHA-256 digest rather than the reusable credential, binds
each entry to a server-supplied player ID, limits credential lifetime to one
second through one hour, supports revocation and caps the number of stored
sessions. Its default capacity is 4,096. Expired entries are removed during
validation or before another credential is issued. Public operations are
mutex-protected.

The caller receives the plaintext credential once and must protect it. The
store is not thread-safe and is not connected to a login flow or the frame
reader. It does not establish a player's identity by itself.

## Transport decision and remaining work

The service uses TCP over TLS 1.3. Reliable ordered delivery fits the current
low-rate input protocol and avoids a custom UDP reliability or congestion
layer. TCP head-of-line blocking may affect gameplay latency; measure it before
considering another transport. Asio 1.36.0 uses Boost Software License 1.0.
OpenSSL 3.6.1 uses Apache License 2.0.

The server stops accepting and ticking before it closes active connections. It
closes unauthenticated sockets immediately and attempts TLS shutdown for
authenticated connections with a 250 ms deadline. The service sends no state
snapshots or acknowledgements to clients. The tests do not measure overload
capacity, connection churn, latency percentiles or long-run resource use.
