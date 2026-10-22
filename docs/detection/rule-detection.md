# Rule-based protocol detection

## Scope

The current server exposes movement intent and basic session controls. The
detector records three violations the server can verify directly:

| Signal | Evidence |
|---|---|
| `movement_out_of_range` | An authenticated frame contains a movement component outside the normalized range. |
| `replayed_sequence` | An authenticated frame reuses or precedes an accepted sequence number. |
| `movement_rate_exceeded` | An authenticated connection exceeds its configured token-bucket rate. |

These signals describe protocol violations. They do not establish cheating or
intent. The server still rejects the frame and closes the connection under its
existing protocol controls.

## Findings and policy

Each finding has schema version 1, a player ID, the signal and its plain-language
reason, the simulation tick, and the count for that signal within the configured
tick window. The default window is 120 ticks. Separate signals have separate
counters. The detector retains history for up to 1,024 players and stores at
most the disconnect threshold of event ticks per signal. The count saturates at
that threshold. The configurable player cap cannot exceed 1,024, and the
disconnect threshold cannot exceed 128. A finding marks
`other_player_history_evicted` when it replaces an older player's history to
stay within the player cap.

The default recommendations progress at 2, 4 and 6 observations in the window:
review, throttle and disconnect. The first observation recommends observation.
The detector always reports `applied_action=observe`; it does not ban, throttle
or disconnect accounts. Protocol-level rejection and connection closure remain
separate controls and continue to apply. `RuleDetectorConfig` allows tests or a
caller to set the window, thresholds and history cap.

`NetworkServiceConfig::on_detection` is required and receives findings on the
service strand. The callback must be quick, non-blocking and non-throwing.
Persisting evidence, operator review, account-level enforcement and
callback-failure isolation are not implemented.

## Verification and limits

GoogleTest covers evidence fields, signal separation, per-player counts,
threshold progression, tick-window expiry, bounded history, invalid input and
integration with authenticated TLS traffic. Test-mode recommendations are
verified to leave applied action at `observe`.

The tests use controlled protocol cases; they are not a labeled gameplay
dataset. No real-player behavior or real-cheat coverage has been measured.
The separate offline movement baseline and its synthetic test limits are
documented in [movement statistics](movement-statistics.md).
