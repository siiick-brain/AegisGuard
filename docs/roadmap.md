# Roadmap

This roadmap separates implemented behavior from work that remains. It does
not promise a release date.

## Implemented

- Server-authoritative movement with fixed-timestep updates.
- Bounded TCP over TLS 1.3 protocol with authenticated sessions.
- Server-side FIRE validation, health, bounded rewind and timed respawn.
- Quadtree spatial queries and deterministic benchmark workloads.
- Eight protocol, movement and rate-limit signals with observe-only findings.
- Bounded in-memory replay and offline movement analysis on synthetic traces.

## Planned

- Integrate with a specific game's server loop, player lifecycle and replication path.
- Match movement, combat, collision, respawn and match rules to the target game.
- Add outbound state replication and client-facing acknowledgments through the
  selected game's existing network protocol.
- Define production credential issuance and certificate provisioning plus
  rotation before any public deployment.
- Evaluate detection thresholds against representative, consented gameplay data.
- Verify load, soak, recovery, sanitizer, fuzzing and container security checks
  on supported release environments.
- Define telemetry retention and deletion controls before collecting persistent
  player data.

No separate health or hit-validation feature is listed here because that
simulation behavior is already implemented and committed. Connecting it to a
specific game remains planned.
