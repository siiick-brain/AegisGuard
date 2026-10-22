# Performance test plan

## Existing measurement

Google Benchmark measures Quadtree insert, remove, update and range-query work
for uniform and clustered synthetic distributions. A brute-force scan provides
the query baseline. The [report](../operations/spatial-benchmarks.md) records
compiler, hardware, entity counts, query widths and five-run medians.

The measurements cover one development machine. They do not establish service
capacity or production performance. Tail latency, memory use, simulation tick
time, network throughput, connection churn and concurrent load remain
unmeasured.

## Next measurements

After the service workload is specified, add a protocol-aware local load
generator. Record machine and compiler details plus offered load, duration,
connection count, error rate and CPU/memory use. Measure tick duration,
packet-processing latency and p50/p95/p99 values. Include incremental load,
bursts, overload and recovery; do not infer game-server capacity from HTTP
throughput.
