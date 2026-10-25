include(FetchContent)

set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(RC_ENABLE_GTEST ON CACHE BOOL "" FORCE)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.17.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(googletest)

FetchContent_Declare(
    rapidcheck
    GIT_REPOSITORY https://github.com/emil-e/rapidcheck.git
    GIT_TAG 2c3c4365aca21ef4e612768fdc95b1ce8b39a651
    GIT_SHALLOW FALSE
)
FetchContent_MakeAvailable(rapidcheck)

add_executable(aegisguard_simulation_tests
    unit/simulation_test.cpp
    unit/quadtree_test.cpp
    unit/protocol_test.cpp
    unit/replay_session_test.cpp
    unit/rule_detector_test.cpp
    unit/network_service_test.cpp
    unit/movement_statistics_test.cpp
    unit/movement_anomaly_test.cpp
    unit/intent_queue_test.cpp
    unit/ip_rate_limiter_test.cpp
    unit/session_store_test.cpp
    unit/server_config_test.cpp
    fuzz/fuzz_regression_test.cpp
)
target_compile_definitions(aegisguard_simulation_tests PRIVATE
    AEGISGUARD_FUZZ_REGRESSION_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fuzz/regression"
)

add_executable(aegisguard_property_tests
    property/quadtree_property_test.cpp
    property/protocol_property_test.cpp
    property/rate_limit_property_test.cpp
    property/session_state_property_test.cpp
)
target_link_libraries(aegisguard_property_tests
    PRIVATE aegisguard_simulation rapidcheck_gtest GTest::gtest_main
)
if(MSVC)
    target_compile_options(aegisguard_property_tests PRIVATE /W4 /WX /permissive-)
else()
    target_compile_options(aegisguard_property_tests
        PRIVATE -Wall -Wextra -Wpedantic -Werror
    )
endif()

add_executable(aegisguard_json_log_probe unit/json_log_probe.cpp)
target_link_libraries(aegisguard_json_log_probe PRIVATE aegisguard_simulation)
if(MSVC)
    target_compile_options(aegisguard_json_log_probe PRIVATE /W4 /WX /permissive-)
else()
    target_compile_options(aegisguard_json_log_probe PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

target_link_libraries(aegisguard_simulation_tests
    PRIVATE aegisguard_simulation GTest::gtest_main
)

if(MSVC)
    target_compile_options(aegisguard_simulation_tests PRIVATE /W4 /WX /permissive-)
else()
    target_compile_options(aegisguard_simulation_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

include(GoogleTest)
gtest_discover_tests(aegisguard_simulation_tests)
gtest_discover_tests(aegisguard_property_tests)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(
    NAME aegisguard_python_tests
    COMMAND ${Python3_EXECUTABLE} -m pytest -q
        ${PROJECT_SOURCE_DIR}/tests/python
)
set_tests_properties(aegisguard_python_tests PROPERTIES
    ENVIRONMENT "AEGISGUARD_BUILD_DIR=${CMAKE_BINARY_DIR}"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
)
