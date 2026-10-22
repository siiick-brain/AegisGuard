# Resilience test plan

## Verified locally

- A client that does not complete authentication is closed at the configured
  deadline.
- A client that sends a frame header but withholds the payload is closed at the
  frame deadline.
- The configured connection cap rejects additional accepted sockets.
- The token bucket rejects movement traffic above its configured rate.
- Invalid authentication, malformed frames and replayed sequences close the
  connection without changing player state.
- Service shutdown cancels accept and tick operations and bounds TLS shutdown.

These are focused loopback tests, not a load or chaos assessment.

## Deferred cases

There is no database, cache, message broker, telemetry exporter, model service,
container deployment or persistent evidence store. Fault tests for those
components, disk exhaustion, restart recovery, backup/restore and long-duration
soak behavior belong to later milestones.
