# Synthetic movement evaluation

This report uses generated traces only. It contains no real-player data and
does not estimate detection quality in a game.

## Dataset and procedure

Run `python -m tools.evaluate` to reproduce the default evaluation. The
generator creates 60-step traces for `legit`, `legit_hard`, `speed`,
`teleport` and `aim` cases. Each class has 40 evaluation traces.

Training used seed `20261025` and 80 legitimate traces: 40 `legit` and 40
`legit_hard`. Evaluation used the separate seed `20261026` and 200 traces:
40 per class. The evaluator rejects equal seeds. It does not pass evaluation
traces or labels to model fitting.

The deterministic rules use fixed thresholds of 5.75 units per second for
speed, 10 units per step for teleport distance and 3.0 radians for aim change.
They were fixed before evaluation and were not tuned on the evaluation seed.
The Isolation Forest uses 100 estimators, `contamination="auto"` and random
state 0. It is trained only on the training-seed legitimate traces. The
recorded run used scikit-learn 1.9.1.

## Measured results

| Method | TP | FP | FN | TN | Precision | Recall | False-positive rate | False-negative rate |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Fixed rules | 120 | 0 | 0 | 80 | 1.0000 | 1.0000 | 0.0000 | 0.0000 |
| Isolation Forest | 120 | 36 | 0 | 44 | 0.7692 | 1.0000 | 0.4500 | 0.0000 |

All 36 Isolation Forest false positives were legitimate synthetic traces:
22 `legit` and 14 `legit_hard`. The fixed rules flagged 40 of 40 traces in
each constructed cheat class and none of the 80 legitimate traces.

These results describe only the generated patterns and the specified seeds.
The Isolation Forest result does not establish generalization. Neither method
is connected to live enforcement. No threshold was selected using the
evaluation data.

## Tests

`python -m pytest tests/python/test_ml_evaluation.py -q` runs the Python
regression and Hypothesis checks. They cover rule behavior, legitimate traces,
seed separation, metric invariants, deterministic generation and JSON report
serialization.

The earlier C++ movement-statistics evaluation is a separate offline
repetition heuristic. Its results are documented in
[movement statistics](../detection/movement-statistics.md). The Python
Isolation Forest is an evaluation baseline only and is not deployed.
