# In-memory replay sessions

## Behavior

`ReplaySession` records schema-versioned ticks for a caller-supplied initial
player list and simulation configuration. Each tick contains only the accepted
movement inputs. A rejected simulation step is not added to the recording and
does not advance session state.

`validate_and_replay()` rebuilds the initial simulation, checks each tick's
schema version and sequence, reapplies the inputs, then returns the final tick
and player states. Callers can inspect a read-only span of recorded ticks or
query the current session state.

This utility is not connected to the TLS service and does not collect network
frames, detection findings or wall-clock telemetry. It has no file format
reader, export path or persistence layer.

## Resource limits

The defaults allow 4,096 ticks and 128 players per session. Hard limits are
100,000 ticks, 1,024 players and 1,000,000 possible player-input records per
session. A tick accepts at most one input per initial player. The object
preallocates its tick capacity, rejects writes after its configured limit and
replays only the bounded recording.

## Data and retention

A recording contains numeric player IDs, initial positions, tick numbers and
accepted movement vectors. Player IDs may be linkable to other application
data; callers should use synthetic or otherwise minimized IDs for test traces.
The session does not collect credentials, addresses, device identifiers or
wall-clock timestamps.

Records remain in process memory until the `ReplaySession` is destroyed. The
implementation does not persist, export, encrypt, pseudonymize or securely
erase them. There is no cryptographic integrity protection. Do not use this
test-session recorder as an operational audit log.

## Verification

Run the focused tests from the repository root:

```powershell
ctest --test-dir build --output-on-failure -R ReplaySessionTest
```

The tests cover accepted recording, replayed final state, rejected inputs,
tick capacity, per-tick input limits and invalid initial configuration.
