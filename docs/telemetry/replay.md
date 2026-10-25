# In-memory replay sessions

## Behavior

`ReplaySession` records schema-versioned ticks for a caller-supplied initial
player list and simulation configuration. Each tick contains only the accepted
movement inputs. A rejected simulation step is not added to the recording and
does not advance session state.

`validate_and_replay()` rebuilds the initial simulation and checks each tick's
schema version and sequence. It reapplies the inputs then returns the final
tick and player states. Callers can inspect a read-only span of recorded ticks or
query the current session state.

This utility is not connected to the TLS service. It does not collect network
frames or detection findings. It does not collect wall-clock telemetry. It has
no file format reader or export path. There is no persistence layer.

## Resource limits

The defaults allow 4096 ticks and 128 players per session. Hard limits allow
100000 ticks and 1024 players. They also limit each session to 1000000
possible player-input records. A tick accepts at most one input per initial
player. The object preallocates its tick capacity and rejects writes after its
configured limit. It replays only the bounded recording.

## Data and retention

A recording contains numeric player IDs and initial positions. It also
contains tick numbers and accepted movement vectors. Player IDs may be linkable
to other application data. Callers should use synthetic or otherwise
minimized IDs for test traces. The session does not collect credentials or
addresses. It does not collect device identifiers or wall-clock timestamps.

Records remain in process memory until the `ReplaySession` is destroyed. The
implementation does not persist or export them. It does not encrypt them or
pseudonymize them. It does not securely erase them. There is no cryptographic
integrity protection. Do not use this
test-session recorder as an operational audit log.

## Verification

Run the focused tests from the repository root:

```powershell
ctest --test-dir build --output-on-failure -R ReplaySessionTest
```

The tests cover accepted recording and replayed final state. They also cover
rejected inputs and tick capacity plus per-tick input limits and invalid initial
configuration.
