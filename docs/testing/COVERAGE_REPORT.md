# Coverage report

## Scope

The threshold applies to the core source modules under `server/src`. It
includes the protocol, movement and anti-cheat logic, spatial index, replay,
session store, simulation and network service. The executable entry point
(`main.cpp`) and configuration adapter (`server_config.cpp`) are outside this
core-module gate. Compiler-generated exception edges are excluded because
they are not application decision branches. The line and branch thresholds
remain 90 percent and 80 percent.

## Latest local measurement

The dedicated GCC coverage build was cleaned and rebuilt then run after all
current configuration, aim-grace and reconnect regression tests were
present. The full CTest suite and coverage thresholds passed from this fresh
profile. An earlier instrumented run had marked the concurrent session-store
and Python CTest targets as timed out; those timeouts did not recur after the
coverage tree was rebuilt.

```powershell
cmake -S . -B build-audit-coverage-ag03 -G Ninja -DCMAKE_BUILD_TYPE=Debug -DAEGIS_COVERAGE=ON -DBUILD_TESTING=ON
cmake --build build-audit-coverage-ag03 --target clean
cmake --build build-audit-coverage-ag03 --parallel 2
ctest --test-dir build-audit-coverage-ag03 --output-on-failure --no-tests=error
python -m gcovr -j 1 -r . build-audit-coverage-ag03 --filter 'server/src/.*' --exclude 'server/src/main.cpp' --exclude 'server/src/server_config.cpp' --exclude-throw-branches --fail-under-line 90 --fail-under-branch 80 --print-summary
```

## Measured result and remaining limitations

| Metric | Measured | Required | Result |
|---|---:|---:|---|
| Line coverage | 91.5% (1251/1367) | 90% | Threshold pass |
| Branch coverage | 81.0% (998/1232) | 80% | Threshold pass |
| Function coverage | 97.5% (116/119) | Informational | Reported |
| Fresh full coverage CTest | 125/125 passed in 70.58 seconds | All tests pass | Pass |
| Earlier coverage CTest | 121 passed; 2 timeout-marked (123 total) | All tests pass | Historical; did not recur after rebuild |

The fresh full run included the concurrent session-store test (0.95 seconds)
and the Python CTest target (21.24 seconds). The Python suite reported 28
tests and two subtests. A separate focused coverage run also passed the
configuration, aim-grace and reconnect tests 3/3. Remote CI coverage remains
unverified until the updated workflow runs.
