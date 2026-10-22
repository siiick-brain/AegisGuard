# ML evaluation status

No ML model or training corpus is implemented, and the project has no
real-player data. Milestone 6 adds a versioned offline movement feature
extractor and a configurable repetition heuristic. Its four labeled examples
are synthetic unit-test traces, not a training corpus or real-world evaluation.
Milestone 7 adds a seeded generator and a disjoint synthetic holdout set. The
holdout has 40 traces per class and is evaluated using the threshold already
defined by the heuristic. It is not used to tune a model or threshold. See
[movement statistics](../detection/movement-statistics.md) for the generator,
feature definition and measured results.

No model was added. The current single-feature heuristic is already
deterministic, cheap to evaluate and has no training or inference dependency to
compare against. A learned model is not justified by these synthetic patterns.
Before considering one, define a representative labeled corpus and a
leakage-safe session split. Validate feature schemas, missing and non-finite
values, label balance, reproducibility, serialization, inference failures and
future-information leakage. Compare a model with the deterministic baseline
and report precision, recall, false-positive rate, false-negative rate and
threshold sensitivity.

No learned-model metrics have been measured.
