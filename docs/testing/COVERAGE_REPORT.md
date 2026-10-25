# Coverage report

## Scope

The threshold applies to the core source modules under `server/src`. It
includes the protocol, movement and anti-cheat logic, spatial index, replay,
session store, simulation and network service. The executable entry point
(`main.cpp`) and configuration adapter (`server_config.cpp`) are outside this
core-module gate. Compiler-generated exception edges are excluded because
they are not application decision branches. The line and branch thresholds
remain 90 percent and 80 percent.

## Command

The local run used GCC with the CMake coverage option, then ran the full CTest
suite before collecting coverage:

```powershell
cmake -S . -B build-cov-local -G Ninja -DCMAKE_BUILD_TYPE=Debug -DAG_ENABLE_COVERAGE=ON -DBUILD_TESTING=ON
cmake --build build-cov-local --parallel 2
ctest --test-dir build-cov-local --output-on-failure --no-tests=error
python -m gcovr -r . --exclude 'tests/.*' --exclude 'build-cov-local/.*' --filter 'server/src/.*' --exclude 'server/src/main.cpp' --exclude 'server/src/server_config.cpp' --exclude-throw-branches --fail-under-line 90 --fail-under-branch 80 --json-summary artifacts/coverage.json --html-details artifacts/coverage.html --print-summary
```

## Measured result

| Metric | Measured | Required | Result |
|---|---:|---:|---|
| Line coverage | 92.3% (1124/1218) | 90% | Pass |
| Branch coverage | 80.8% (891/1103) | 80% | Pass |
| Function coverage | 97.4% (112/115) | Informational | Reported |
| CTest | 121/121 passed in 34.38 seconds | All tests pass | Pass |

The gcovr JSON summary and HTML detail report were generated under the ignored
`artifacts/` directory. The CI workflow uses the same filter and thresholds.
