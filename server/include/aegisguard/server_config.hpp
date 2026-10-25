#pragma once

#include "aegisguard/simulation.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace aegisguard {

struct ServerConfig {
    std::string bind_address{"127.0.0.1"};
    std::uint16_t port{8443};
    std::uint16_t admin_port{8444};
    SimulationConfig simulation{};
    std::size_t maximum_connections{128};
    std::chrono::milliseconds authentication_timeout{3000};
    std::chrono::milliseconds frame_timeout{1000};
    double movement_rate_capacity{16.0};
    double movement_rate_per_second{60.0};
    double ip_rate_capacity{128.0};
    double ip_rate_per_second{120.0};
    std::size_t maximum_tracked_ip_rates{4096};
    std::size_t maximum_queued_intents_per_player{64};
    std::size_t maximum_sessions{4096};
    std::string tls_certificate;
    std::string tls_private_key;
    std::string admin_ca_certificate;
    std::string token_file;
    std::uint64_t demo_player_id{7};
    Vec2 initial_position{10.0, 10.0};
};

[[nodiscard]] ServerConfig parse_server_config(std::string_view json);

}  // namespace aegisguard