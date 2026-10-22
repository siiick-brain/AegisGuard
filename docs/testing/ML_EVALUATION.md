# ML evaluation status

No ML model, feature pipeline or training corpus is implemented. The project
has no real-player data. Synthetic evaluations must be labeled as synthetic and
must not be reported as real-world cheat-detection accuracy.

Before introducing a model, define a leakage-safe session split and a labeled
corpus. Validate feature schemas, missing and non-finite values, label balance,
reproducibility, serialization, inference failures and future-information
leakage. Compare the model with deterministic rules and report precision,
recall, false-positive rate, false-negative rate and threshold sensitivity.

No ML metrics have been measured.
