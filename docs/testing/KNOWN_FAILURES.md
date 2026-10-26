# Known failures and open limitations

Historical: A coverage run timed out the session-store and Python tests; a
clean rebuild later passed 125/125, and the original timeout cause is unknown.
Historical: One Debug Python run took 1498.93 seconds; isolated and later full
runs passed normally, and the cause of that outlier is unknown.

No current local Debug, Release, or clean coverage CTest failure is known. See
the coverage report for current verification.

Open limitations:

- Clang/libFuzzer and sanitizer runs have not been completed locally; the
  configured fuzz CI job has no recorded remote result.
- Load/soak, fault recovery, production certificate/key rotation, and broader
  HTTP security testing remain unverified.
- No Godot client or complete multiplayer match lifecycle exists; gameplay
  detection has no representative real-player data or measured false-positive
  rate.
- Docker/Trivy and dependency/secret scanning results remain unavailable.
