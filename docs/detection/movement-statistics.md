# Movement statistics baseline

## Scope

Milestone 6 adds offline analysis for movement inputs already captured in a
versioned `ReplaySession` trace. It does not read live network traffic or change
the network service's decisions.

Feature schema version 1 records the player's input sample count, the number of
adjacent tick pairs with input in both ticks, the count of identical movement
vectors in those pairs, the fraction of pairs that repeat, and the mean and
population standard deviation of normalized movement magnitude. Magnitude is
capped at 1 to match the simulation's movement normalization. A tick without
input breaks an adjacent pair. Movement vectors count as identical when both
components compare equal.

The default heuristic requires at least 20 samples and a repeated-input
fraction of 0.9 or greater. Both values are configurable. A finding records
the measured fraction and threshold. The fraction is a feature value, not a
probability or a calibrated risk score. Findings use schema version 1 and
always report `applied_action=observe`.

## Verification

GoogleTest checks feature values, malformed traces, missing ticks, invalid
configuration and the observe-only finding. Its labeled evaluation test uses
four deterministic synthetic traces: two varied movement traces labelled
non-repetitive and two constant-input traces labelled repetitive. With the
default threshold, the test observes 2 true positives, 2 true negatives, no
false positives and no false negatives among those four examples.

These examples verify the implementation against deliberately distinct
patterns. They are not representative gameplay sessions, a training corpus or
an estimate of real-world precision, recall or false-positive rates.

Milestone 7 adds a seeded generator for 40-tick traces. Its non-repetitive
class changes direction after randomly selected holds of one to four ticks.
Its repetitive class holds a selected direction for the entire trace. A
development set and holdout set each contain 40 traces per class. Their seed
ranges are disjoint, and the default threshold was fixed before evaluating the
holdout. The test confirms that generation is deterministic for a given seed.
Run the evaluation with
`ctest --test-dir build -R MovementStatisticsEvaluationTest --output-on-failure`.

The holdout produced 40 true positives, 40 true negatives, no false positives
and no false negatives. Precision and recall were 1.0; false-positive and
false-negative rates were 0.0. These are measured results for the generated
patterns only. The classes were deliberately constructed to separate constant
input from short directional holds, so these rates say nothing about player
behavior or cheating in a game.

## Limitations

Legitimate play can include repeated movement, such as holding a direction or
waiting in place. The heuristic can therefore flag ordinary behavior. It does
not account for game mode, player intent, input device, latency or match
context. The project has no playable game, real-player traces, validated labels
or calibration study. Treat findings as a prompt for review, not evidence of
cheating. Live-service integration and enforcement remain deferred until a
game supplies suitable events and validation data.
