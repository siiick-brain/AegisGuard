# Protocol fuzzing

## Target

`tests/fuzz/protocol_fuzz.cpp` passes arbitrary byte spans to
`decode_move_frame`. The decoder has a fixed header and payload size; it should
return a protocol error for malformed data without allocating from the client
length field.

The target compiles `server/src/protocol.cpp` directly so libFuzzer coverage
instruments the parser. AddressSanitizer and UndefinedBehaviorSanitizer are
enabled with the fuzzer.

## Build and run

The supported configuration is Clang on Linux or macOS:

```bash
cmake -S . -B build-fuzz -G Ninja \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DAG_BUILD_FUZZERS=ON \
  -DBUILD_TESTING=OFF
cmake --build build-fuzz --target aegisguard_protocol_fuzz
mkdir -p artifacts/fuzz
./build-fuzz/aegisguard_protocol_fuzz \
  -max_len=64 \
  -max_total_time=60 \
  -artifact_prefix=artifacts/fuzz/
```

Set a longer `-max_total_time` for scheduled fuzzing. Keep minimized crashes as
regression fixtures before changing the parser. Do not fuzz services outside
the local test environment.

## Result status

The target is configured in CMake. This Windows environment has GCC 15.2.0 but
no Clang installation. Compilation and execution are deferred. No fuzz
coverage or crash-free claim is made. The current harness covers only movement
frame decoding. It does not fuzz TLS or credentials. Configuration and
persistence are not fuzzed either.
