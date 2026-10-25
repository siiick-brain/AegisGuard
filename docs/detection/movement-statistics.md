# Movement statistics baseline

## Scope

Milestone 6 adds offline analysis for movement inputs already captured in a
versioned `ReplaySession` trace. It does not read live network traffic. It does
not change the network service's decisions.

Feature schema version 1 records the player's input sample count and the number
of adjacent tick pairs with input in both ticks. It also records the count of
identical movement vectors in those pairs and the fraction of pairs that
repeat. The schema includes the mean and population standard deviation of
normalized movement magnitude. Magnitude is capped at 1 to match the
simulation's movement normalization. A tick without
input breaks an adjacent pair. Movement vectors count as identical when both
components compare equal.

The default heuristic requires at least 20 samples with a repeated-input
fraction of 0.9 or greater. Both values are configurable. A finding records
the measured fraction and threshold. The fraction is a feature value. It is
not a probability or a calibrated risk score. Findings use schema version 1 and
always report `applied_action=observe`.

## Verification

GoogleTest checks feature values and malformed traces. It checks missing ticks
and invalid configuration plus the observe-only finding. Its labeled evaluation
test uses four deterministic synthetic traces. Two varied movement traces are labelled
non-repetitive. Two constant-input traces are labelled repetitive. The test
uses the default threshold and observes 2 true positives and 2 true negatives
with zero false positives and zero false negatives among those four examples.

These examples compare the implementation against deliberately distinct
patterns. They are not representative gameplay sessions or a training corpus.
They are not an estimate of real-world precision or recall. They do not
estimate false-positive rates.

Milestone 7 adds a seeded generator for 40-tick traces. Its non-repetitive
class changes direction after random holds of one to four ticks. Its repetitive
class holds a selected direction for the entire trace. A development set and
holdout set each contain 40 traces per class. Their seed
ranges are disjoint. The default threshold was fixed before evaluating the
holdout. The test confirms that generation is deterministic for a given seed.
Run the evaluation with
`ctest --test-dir build -R MovementStatisticsEvaluationTest --output-on-failure`.

The holdout produced 40 true positives and 40 true negatives with zero false
positives and zero false negatives. Precision and recall were 1.0.
False-positive and false-negative rates were 0.0. These are measured results
for the generated
patterns only. The classes were deliberately constructed to separate constant
input from short directional holds. These rates say nothing about player
behavior or cheating in a game.

## Limitations

Legitimate play can include repeated movement. A player may hold a direction
or wait in place. The heuristic can therefore flag ordinary behavior. It does
not account for game mode or player intent. It also ignores input device
latency and match context. The project has no playable game or real-player
traces. It has no validated labels or calibration study. Treat findings as a
prompt for review and not as evidence of cheating. Live-service integration
and enforcement remain deferred until a game supplies suitable events and
validation data.
