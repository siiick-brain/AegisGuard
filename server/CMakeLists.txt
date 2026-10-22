add_library(aegisguard_simulation
    src/network_service.cpp
    src/movement_statistics.cpp
    src/protocol.cpp
    src/replay_session.cpp
    src/rule_detector.cpp
    src/session_store.cpp
    src/simulation.cpp
    src/quadtree.cpp
)

target_include_directories(aegisguard_simulation
    PUBLIC include
)

target_include_directories(aegisguard_simulation SYSTEM PUBLIC ${ASIO_INCLUDE_DIR})
target_compile_definitions(aegisguard_simulation PUBLIC ASIO_STANDALONE)
if(MINGW)
    target_compile_definitions(aegisguard_simulation PUBLIC ASIO_HAS_PTHREADS=1)
endif()
target_link_libraries(aegisguard_simulation PUBLIC OpenSSL::SSL OpenSSL::Crypto)

if(WIN32)
    target_compile_definitions(aegisguard_simulation PUBLIC _WIN32_WINNT=0x0A00)
    target_link_libraries(aegisguard_simulation PUBLIC mswsock ws2_32)
endif()

target_compile_features(aegisguard_simulation PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(aegisguard_simulation PRIVATE /W4 /WX /permissive-)
else()
    target_compile_options(aegisguard_simulation PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
