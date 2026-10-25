# ML evaluation status

No ML model or training corpus is implemented. The project has no
real-player data. Milestone 6 adds a versioned offline movement feature
extractor and a configurable repetition heuristic. Its four labeled examples
are synthetic unit-test traces. They are not a training corpus or a
real-world evaluation.
Milestone 7 adds a seeded generator and a disjoint synthetic holdout set. The
holdout has 40 traces per class and is evaluated using the threshold already
defined by the heuristic. It is not used to tune a model or threshold. See
[movement statistics](../detection/movement-statistics.md) for the generator
feature definition and measured results.

No model was added. The current single-feature heuristic is already
deterministic and cheap to evaluate. It has no training or inference dependency to
compare against. A learned model is not justified by these synthetic patterns.
Before considering one define a representative labeled corpus and a
leakage-safe session split. Validate feature schemas plus missing and
non-finite values. Check label balance and reproducibility. Validate
serialization and inference failures plus future-information leakage. Compare
a model with the deterministic baseline. Report precision and recall plus the
false-positive rate and false-negative rate and threshold sensitivity.

No learned-model metrics have been measured.
