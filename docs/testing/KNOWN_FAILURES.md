# Known failures and unverified checks

## MinGW Asio thread-model mismatch

**Status: fixed in the current worktree and stress-verified locally.**
The crash was traced to inconsistent standalone Asio thread-backend selection
across translation units in the MinGW build. `ASIO_HAS_PTHREADS` was defined
when the network-service header included Asio, but undefined when
`protocol.hpp` first included Asio in `protocol_test.cpp`. Because Asio uses
this macro to select the internal thread type for its Windows IOCP context,
mixing these definitions violated the One Definition Rule and could corrupt
`io_context` teardown.

### Before the fix

On 2026-10-22, before the CMake fix, ten consecutive full CTest runs produced:

```text
Run 1:  2 failures — RejectsConnectionsAboveTheConfiguredLimit; StopsDuringHandshakeWithoutStartingTlsShutdown
Run 2:  70/70 passed
Run 3:  70/70 passed
Run 4:  1 failure  — StopsDuringHandshakeWithoutStartingTlsShutdown
Run 5:  1 failure  — TracksRepeatedReplaySignalsAcrossConnections
Run 6:  1 failure  — RejectsTlsTwelveClients
Run 7:  2 failures — ClosesPartialFramesAfterTheReadDeadline; RejectsTlsTwelveClients
Run 8:  1 failure  — RejectsTlsTwelveClients
Run 9:  1 failure  — RejectsConnectionsAboveTheConfiguredLimit
Run 10: 70/70 passed
Overall: 3/10 runs passed; 7/10 failed (9 network-test segfaults across failed runs).
```

Failures were reported by CTest as `***Exception: SegFault`. GDB later captured
faults in
`asio::detail::win_iocp_io_context::timer_thread_function`, including access
to an address matching the Windows freed-heap pattern, while test teardown was
joining the service thread. Preprocessing confirmed the differing
`ASIO_HAS_PTHREADS` definitions.

### Fix and post-fix validation

The simulation target now publishes `ASIO_HAS_PTHREADS=1` on MinGW, so the
library and linked consumers compile Asio code with the same thread type. MSVC
and other toolchains are not forced to use pthreads. TLS handshake, shutdown
and ownership logic were not changed.

After rebuilding both Release and Debug configurations on 2026-10-22:

- The three previously problematic focused tests each passed 20 consecutive
  repetitions in Debug (60/60 total).
- The complete 70-test CTest suite passed 10/10 consecutive runs in Debug
  (700/700 test executions).
- The complete 70-test CTest suite passed 10/10 consecutive runs in Release
  (700/700 test executions).
- The Python utility suite passed 2/2.

The earlier crash was not reproduced in these post-fix runs. This is strong
local evidence that the build-configuration fix addresses the failure on this
Windows MinGW toolchain; it is not a guarantee for other toolchains or systems.

On 2026-10-22, the current worktree was rebuilt successfully with:

```powershell
cmake --build build --parallel 2
```

The 53 non-network C++ tests passed with:

```powershell
.\build\tests\aegisguard_simulation_tests.exe --gtest_filter=-NetworkService*
```

The two Python utility tests also passed with:

```powershell
python -m unittest discover -s tests/python -p "test_*.py"
```

A focused run of the three TLS lifecycle cases produced mixed results:

```powershell
.\build\tests\aegisguard_simulation_tests.exe --gtest_filter=NetworkServiceTest.StopsAnAuthenticatedConnectionWithTlsShutdown:NetworkServiceTest.StopsDuringHandshakeWithoutStartingTlsShutdown:NetworkServiceTest.RetainsContextsUntilShutdownHandlersHaveDrained
```

Before the fix, the authenticated-shutdown and stop-during-handshake cases
reported `[ OK ]`, then the process terminated while starting the
context-retention case. The Windows process exit code was `-1073741819`
(`0xC0000005`, access violation). The context-retention case passed when run
alone. Subsequent GDB and macro-preprocessing evidence isolated the mismatch
described above. The post-fix repetitions are recorded in “Fix and post-fix
validation.”

Earlier full-suite runs also had intermittent process crashes: Release passed
67/68 tests and Debug passed 66/68. Failures varied and included
`NetworkServiceConfigTest.RequiresDetectionHandlerToAvoidDroppingFindings`,
`NetworkServiceTest.RejectsMalformedFrameWithoutChangingPlayerState`, and
`NetworkServiceTest.RejectsConnectionsAboveTheConfiguredLimit`. Some crashes
occurred after GoogleTest had printed passing results. GDB showed a hang during
`asio::io_context` destruction while joining a Windows Asio IOCP thread, and a
separate run faulted in `_InterlockedExchange` with an address matching the
Windows freed-heap pattern. These are observations, not a confirmed root cause.

TLS tests generate an ephemeral certificate in-process and disable
verification in the test client. The installed `mkcert` certificate is for
local external clients and does not affect these tests. The tests therefore do
not validate production certificate trust, deployment, or key rotation.

### Expected TLS result before calling this fixed

- Valid TLS 1.3 clients complete the handshake and exercise the authenticated
  protocol; TLS 1.2 clients are rejected.
- Stopping a connected session completes asynchronous TLS shutdown before its
  socket and session are destroyed.
- Stopping during a handshake does not start TLS shutdown concurrently with
  that handshake.
- TLS context and `io_context` remain alive until all sessions and asynchronous
  callbacks have completed.
- Focused lifecycle tests and the complete C++ test suite exit successfully
  across repeated runs, with no access violation or teardown hang.

These lifecycle properties are covered by the focused integration tests. The
thread-model correction did not change TLS shutdown logic.

## Project execution status

The current source and test targets build, and the non-network tests above
pass. AegisGuard is not yet a runnable standalone game server: CMake currently
builds the simulation/network code as a library, plus test and optional
benchmark executables. There is no server entry point, client, deployment
configuration, or production certificate setup. Local TLS integration tests
run a test service and client on loopback; that is not equivalent to running a
complete game.

The following checks are unverified or unavailable:

- libFuzzer compilation and execution; Clang is not installed locally.
- Line and branch coverage; `gcov` is installed but a report has not been
  generated.
- ThreadSanitizer and other LLVM sanitizers.
- Trivy, Dependency-Check, container, secret and infrastructure scans. CI
  workflows are configured, but no GitHub workflow run or scan result exists
  yet.
- Service load, soak, fault-injection, backup and restore tests.
- Dashboard/API authorization, accessibility and ZAP testing.
- Real gameplay traces, calibrated statistical detection and ML evaluation.

These are limitations, not confirmed vulnerabilities or clean scan results.
