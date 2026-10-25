# Property and differential testing

RapidCheck is pinned to commit
`2c3c4365aca21ef4e612768fdc95b1ce8b39a651`. Seven registered properties
compare quadtree operations, movement-frame round trips, arbitrary decoder
bytes, HELLO versions, token-bucket budgets, sequence acceptance and session
state transitions against expected invariants or reference models.

The Quadtree property generates insert, update, remove and query operations
and compares each result with a `std::map` reference model. A separate
deterministic test uses seed `0xA3615` for 600 operations. The protocol
properties check round trips, malformed input handling and strict sequence
ordering. The rate and session properties use injected deterministic time.

Run the properties with
`ctest --test-dir build-cov-local --output-on-failure -R Property`. The latest
run passed 7/7 tests in 3.83 seconds. RapidCheck prints its reproduction seed
when a property fails. Keep a reduced named GoogleTest regression for each
production counterexample.
