# Property and differential testing

The Quadtree test uses a fixed seed (`0xA3615`) and performs 600 generated
insert, update, remove and query steps. After each step, a simple `std::map`
scan acts as an independent oracle for the query result and entity count.
Boundary, duplicate-position, degenerate-range and invalid-input cases have
separate tests.

Protocol tests currently use specification-built fixtures and negative cases.
The libFuzzer harness supplements them when a supported Clang environment is
available. RapidCheck is not added because the existing deterministic generator
already covers the Quadtree state sequence; additional dependencies will be
considered when a second stateful property suite exists.

Failure seeds are fixed in the test source. A failing sequence should be
reduced to a small reproducer and retained as a regression case.
