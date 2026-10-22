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
    add_executable(aegisguard_protocol_fuzz
        tests/fuzz/protocol_fuzz.cpp
        server/src/protocol.cpp
    )
    target_include_directories(aegisguard_protocol_fuzz PRIVATE
        server/include
        ${ASIO_INCLUDE_DIR}
    )
    target_compile_definitions(aegisguard_protocol_fuzz PRIVATE ASIO_STANDALONE)
    target_compile_options(aegisguard_protocol_fuzz PRIVATE
        -Wall -Wextra -Wpedantic -Werror
        -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer
    )
    target_link_options(aegisguard_protocol_fuzz PRIVATE
        -fsanitize=fuzzer,address,undefined
    )
endif()
