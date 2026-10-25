# Rule-based protocol detection

## Scope

The current server exposes movement intent and basic session controls. The
detector records eight violation types the server can verify directly:

| Signal | Evidence |
|---|---|
| `movement_out_of_range` | An authenticated frame contains a movement component outside the normalized range. |
| `replayed_sequence` | An authenticated frame reuses or precedes an accepted sequence number. |
| `movement_rate_exceeded` | An authenticated connection exceeds its configured token-bucket rate. |
| `speed_exceeded` | Reported position displacement exceeds the speed bound after latency grace and jitter tolerance. |
| `teleport_distance_exceeded` | Reported position displacement exceeds 10 world units. |
| `aim_snap_exceeded` | Wrapped aim-angle displacement exceeds the configured angular rate and grace interval. |
| `ip_rate_exceeded` | An authenticated source address exceeds its shared token-bucket rate or the tracked-address bound. |
| `movement_intent_queue_full` | A player's bounded pending-intent FIFO reaches its configured capacity. |

These signals describe protocol violations. They do not establish cheating or
intent. The server still rejects the frame and closes the connection under its
existing protocol controls.

## Findings and policy

Each finding has schema version 1 and a player ID. It records the signal and
its plain-language reason plus the simulation tick and count for that signal
within the configured tick window. The default window is 120 ticks. Separate signals
have separate counters. The detector retains history for up to 1024 players.
It stores at
most the disconnect threshold of event ticks per signal. The count saturates at
that threshold. The configurable player cap cannot exceed 1024. The
disconnect threshold cannot exceed 128. A finding marks
`other_player_history_evicted` when it replaces an older player's history to
stay within the player cap.

The default recommendations progress at two observations then at four and
finally at six within the window.
They recommend review then throttle and finally disconnect. The first
observation recommends observation.
The detector always reports `applied_action=observe`. It does not ban or
throttle accounts and does not disconnect them. Protocol-level rejection and
connection closure remain separate controls and continue to apply.
`RuleDetectorConfig` allows tests or a caller to set the window and thresholds.
It also allows setting the history cap.

`NetworkServiceConfig::on_detection` is required and receives findings on the
service strand. The callback must be quick and non-blocking. It must not throw.
Persisting evidence and operator review are not implemented. Account-level
enforcement and callback-failure isolation are not implemented either.

## Verification and limits

GoogleTest covers evidence fields and signal separation. It tests per-player
counts plus threshold progression and tick-window expiry. It tests bounded
history and invalid input. Authenticated TLS traffic is covered by integration
tests. Test-mode recommendations are
verified to leave applied action at `observe`.

The tests use controlled protocol cases. They are not a labeled gameplay
dataset. No real-player behavior or real-cheat coverage has been measured.
Position claims are untrusted telemetry and are never applied to authoritative
state. Default thresholds use a 250 ms position grace, 1.0 world unit of
jitter tolerance, 10 world units for teleport classification and 40 radians
per second with a 20 ms grace for aim. Latency, jitter and fast legitimate
turns have deterministic false-positive tests; no field false-positive rate
is known.
The separate offline movement baseline and its synthetic test limits are
documented in [movement statistics](movement-statistics.md).
