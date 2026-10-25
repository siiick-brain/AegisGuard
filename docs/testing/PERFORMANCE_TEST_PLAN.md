# Performance test plan

## Existing measurement

Google Benchmark measures Quadtree insert/remove/update and range-query work
for uniform and clustered synthetic distributions. A brute-force scan provides
the query baseline. The [report](../operations/spatial-benchmarks.md) records
compiler and hardware details plus entity counts and query widths. It includes
five-run medians.

The measurements cover one development machine. They do not establish service
capacity or production performance. Tail latency and memory use remain
unmeasured. So do simulation tick time and network throughput plus connection
churn and concurrent load.

## Next measurements

After the service workload is specified add a protocol-aware local load
generator. Record machine and compiler details plus offered load and duration.
Also record connection count and error rate plus CPU and memory use. Measure
tick duration and packet-processing latency along with p50/p95/p99 values.
Include incremental load and bursts plus overload and recovery. Do not infer game-server capacity from HTTP
throughput.
