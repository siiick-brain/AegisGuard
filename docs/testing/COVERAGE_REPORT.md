# Coverage report

No line or branch coverage report has been generated yet. GCC's `gcov` is
available in the current MSYS2 UCRT64 toolchain; `gcovr`, Clang coverage tools
and ThreadSanitizer are unavailable.

To collect GCC coverage in a separate build directory:

```powershell
cmake -S . -B build-coverage -G Ninja -DCMAKE_BUILD_TYPE=Debug `
  -DCMAKE_CXX_FLAGS=--coverage -DCMAKE_EXE_LINKER_FLAGS=--coverage
cmake --build build-coverage --parallel
ctest --test-dir build-coverage --output-on-failure
```

The resulting `.gcda` and `.gcno` files are raw coverage data. A report still
needs to be generated and inspected; line execution alone does not show whether
security decisions are meaningfully asserted.
