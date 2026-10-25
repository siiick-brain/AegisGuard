# Test strategy

## Scope

Tests follow the milestone gates in [the requirements matrix](../requirements-matrix.md).
Each test should verify a requirement or security boundary. It should test an
invariant or failure mode plus a measurable property. Passing tests do not establish production
readiness or real-world cheat-detection accuracy.

## Current layers

- GoogleTest covers simulation invariants and session lifecycle. It covers
  protocol parsing and Quadtree operations plus loopback TLS service behavior.
- The Quadtree test uses a seeded operation sequence and an independent
  `std::map` oracle.
- Google Benchmark measures synthetic spatial workloads. Its results are
  documented separately from test results.
- An optional Clang libFuzzer target exercises the frame decoder. The harness
  is present; a fuzz run has not been completed in the current environment.

## Test progression

Fast unit and integration tests run after each implementation change.
Randomized and fuzz checks are added with their owning milestone. Load and
resilience checks plus release checks are also added with the owning milestone.
Unavailable checks remain marked deferred. Failures are retained as regression
tests after their root cause is understood.

The [catalogue](TEST_CATALOG.md) lists every requested test category. The
[matrix](TEST_MATRIX.md) connects current requirements to evidence.
