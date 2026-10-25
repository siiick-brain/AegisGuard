cmake_minimum_required(VERSION 3.25)

project(AegisGuard VERSION 0.1.0 LANGUAGES CXX)

get_filename_component(AG_COMPILER_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
get_filename_component(AG_COMPILER_PREFIX "${AG_COMPILER_BIN}" DIRECTORY)
find_path(ASIO_INCLUDE_DIR NAMES asio.hpp HINTS "${AG_COMPILER_PREFIX}/include" REQUIRED)
if(EXISTS "${AG_COMPILER_PREFIX}/include/openssl/evp.h")
    set(OPENSSL_ROOT_DIR "${AG_COMPILER_PREFIX}")
endif()
find_package(OpenSSL 3.0 REQUIRED COMPONENTS Crypto SSL)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

option(AG_BUILD_BENCHMARKS "Build spatial-engine benchmarks" OFF)
option(AG_BUILD_FUZZERS "Build libFuzzer targets (Clang on Linux/macOS)" OFF)
option(AG_ENABLE_COVERAGE "Enable gcov-compatible coverage instrumentation" OFF)

if(AG_ENABLE_COVERAGE)
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        message(FATAL_ERROR "AG_ENABLE_COVERAGE requires GCC/gcov")
    endif()
    add_compile_options(--coverage -O0 -g)
    add_link_options(--coverage)
endif()

add_subdirectory(server)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

if(AG_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

if(AG_BUILD_FUZZERS)
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR WIN32)
        message(FATAL_ERROR "AG_BUILD_FUZZERS requires Clang on Linux or macOS")
    endif()
    target_compile_options(aegisguard_simulation PRIVATE
        -fsanitize=fuzzer-no-link,address,undefined
        -fno-omit-frame-pointer
    )
    target_link_options(aegisguard_simulation INTERFACE
        -fsanitize=address,undefined
    )
    foreach(fuzz_target IN ITEMS decoder hello config)
        add_executable(aegisguard_fuzz_${fuzz_target}
            tests/fuzz/${fuzz_target}_fuzz.cpp
        )
        target_link_libraries(aegisguard_fuzz_${fuzz_target}
            PRIVATE aegisguard_simulation
        )
        target_compile_options(aegisguard_fuzz_${fuzz_target} PRIVATE
            -Wall -Wextra -Wpedantic -Werror
            -fsanitize=fuzzer,address,undefined
            -fno-omit-frame-pointer
        )
        target_link_options(aegisguard_fuzz_${fuzz_target} PRIVATE
            -fsanitize=fuzzer,address,undefined
        )
        add_test(
            NAME FuzzRegression_${fuzz_target}
            COMMAND aegisguard_fuzz_${fuzz_target}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/fuzz/regression
                -runs=0
        )
    endforeach()
endif()
