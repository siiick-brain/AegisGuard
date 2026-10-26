# Contributing

AegisGuard is a local educational prototype. Contributions must not imply
production readiness or OWASP compliance.

## Build and test

Use CMake 3.25 or newer, Ninja, a C++20 compiler, standalone Asio and OpenSSL
3.0 or newer. The first test configure fetches GoogleTest.

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
python -m pytest -q tests/python
python -m ruff check .
python -m mypy tools
```

Run the focused tests for changed behavior, then the full suite. Preserve valid
tests and add a regression test when fixing a bug. State any checks that were
not run.

## Pull requests

Use the pull request template. Explain the behavior change and list the
commands run plus any checks not run. Keep changes focused and avoid including
unrelated generated files.

## Changes

- Keep changes focused and follow the existing C++20 and CMake conventions.
- Do not add dependencies or tools without maintainer approval.
- Bind network listeners to loopback by default.
- Never commit credentials, private keys, certificates or tokens.
- Document measured results and keep synthetic results distinct from gameplay
  evidence.
- Do not claim production readiness or standards compliance.