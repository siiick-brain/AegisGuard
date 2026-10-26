# Changelog

Changes to this project are recorded here.

## [0.1.0] - 2026-10-26

### Added

- C++20 authoritative movement simulation with fixed-timestep updates.
- TLS 1.3 game service with bounded framing, session validation and rate limits.
- Server-side firing, health, bounded rewind and respawn behavior.
- Quadtree spatial queries, benchmark harness and recorded query results.
- Protocol and movement detection findings with observe-only recommendations.
- Bounded in-memory replay and synthetic offline movement evaluation.
- CTest, Python, property, fuzz-target and coverage checks.

### Changed

- Documentation distinguishes implemented behavior from deferred integration
  work and verification limits.

### Security

- CI actions use immutable commit references.
- CodeQL and Scorecard workflows skip runs when the repository is private.
