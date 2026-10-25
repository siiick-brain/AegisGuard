include(FetchContent)

set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.17.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(googletest)

add_executable(aegisguard_simulation_tests
    unit/simulation_test.cpp
    unit/quadtree_test.cpp
    unit/protocol_test.cpp
    unit/replay_session_test.cpp
    unit/rule_detector_test.cpp
    unit/network_service_test.cpp
    unit/movement_statistics_test.cpp
    unit/session_store_test.cpp
    unit/server_config_test.cpp
)

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
