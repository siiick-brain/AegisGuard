include(FetchContent)

set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    googlebenchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.9.4
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(googlebenchmark)

add_executable(aegisguard_spatial_benchmarks
    spatial_benchmark.cpp
)

target_link_libraries(aegisguard_spatial_benchmarks
    PRIVATE aegisguard_simulation benchmark::benchmark
)

if(MSVC)
    target_compile_options(aegisguard_spatial_benchmarks PRIVATE /W4 /WX /permissive-)
else()
    target_compile_options(aegisguard_spatial_benchmarks PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
