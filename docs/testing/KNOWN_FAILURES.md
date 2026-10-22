# Known failures and unverified checks

The Release build passes all 44 tests. One fresh Debug CTest run reported a
segmentation fault after `NetworkServiceTest.StopsAnAuthenticatedConnectionWithTlsShutdown`
printed that it passed. Before the test change, 50 direct test-process runs did
not reproduce it. The test now completes the client side of the TLS shutdown.
After that change, the issue was not reproduced in 30 isolated CTest repeats or
three repeated Debug full-suite runs. The cause was not established, so
Milestone 3 stays open pending confirmation.

The following checks are unverified or unavailable:

- libFuzzer compilation and execution; Clang is not installed locally.
- Line and branch coverage; `gcov` is installed but a report has not been
  generated.
- ThreadSanitizer and other LLVM sanitizers.
- Container, dependency, secret, web-application and workflow scans.
- Service load, soak, fault-injection, backup and restore tests.
- Dashboard/API authorization, accessibility and ZAP testing.
- Detector and ML evaluation.

These are limitations, not confirmed vulnerabilities or clean scan results.
