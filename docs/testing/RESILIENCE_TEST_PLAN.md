# Resilience test plan

## Verified locally

- A client that does not complete authentication is closed at the configured
  deadline.
- A client that sends a frame header but withholds the payload is closed at the
  frame deadline.
- The configured connection cap rejects additional accepted sockets.
- The token bucket rejects movement traffic above its configured rate.
- Invalid authentication and malformed frames close the connection without
  changing player state. Replayed sequences do too.
- Service shutdown cancels accept and tick operations and bounds TLS shutdown.

These are focused loopback tests. They are not a load or chaos assessment.

## Deferred cases

There is no database or cache. There is no message broker or telemetry
exporter. A model service and container deployment are also absent. There is no
persistent evidence store. Fault tests for those components belong to later
milestones. So do disk exhaustion and restart recovery plus backup/restore and
long-duration soak behavior.
