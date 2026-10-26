# Protocol fuzzing

## Targets

CMake defines three Clang/libFuzzer targets:

| Target | Source | Input coverage |
|---|---|---|
| `aegisguard_fuzz_decoder` | `tests/fuzz/decoder_fuzz.cpp` | Protocol header, movement frame and FIRE frame decoders |
| `aegisguard_fuzz_hello` | `tests/fuzz/hello_fuzz.cpp` | Explicit HELLO decoder |
| `aegisguard_fuzz_config` | `tests/fuzz/config_fuzz.cpp` | Server configuration parser |

The decoder inputs are bounded to 4096 bytes and configuration inputs to
65536 bytes. The parser library is instrumented for libFuzzer coverage, with
AddressSanitizer and UndefinedBehaviorSanitizer enabled.

## Build and run

The supported configuration is Clang on Linux or macOS. The CI job installs
Clang on an Ubuntu 24.04 runner, builds all three targets and runs each against
the checked-in regression corpus with a 15-second time limit:

```bash
cmake -S . -B build-fuzz -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DAG_BUILD_FUZZERS=ON \
  -DBUILD_TESTING=OFF
cmake --build build-fuzz --parallel 2 --target \
  aegisguard_fuzz_decoder \
  aegisguard_fuzz_hello \
  aegisguard_fuzz_config
mkdir -p artifacts/fuzz
./build-fuzz/aegisguard_fuzz_decoder tests/fuzz/regression \
  -max_len=4096 -max_total_time=60 \
  -artifact_prefix=artifacts/fuzz/decoder-
./build-fuzz/aegisguard_fuzz_hello tests/fuzz/regression \
  -max_len=4096 -max_total_time=60 \
  -artifact_prefix=artifacts/fuzz/hello-
./build-fuzz/aegisguard_fuzz_config tests/fuzz/regression \
  -max_len=65536 -max_total_time=60 \
  -artifact_prefix=artifacts/fuzz/config-
```

Use `-runs=0` for a quick regression-corpus replay without mutation. Keep
minimized crashes as regression fixtures before changing a parser. Do not fuzz
services outside the local test environment.

## Result status

The targets are configured in CMake and a bounded CI smoke job is configured.
This Windows environment has GCC 15.2.0 but no Clang installation, so local
compilation and execution were not performed. No CI result is claimed until a
run of the updated workflow is recorded. The harness does not fuzz TLS,
credentials, persistence or the live network service.
