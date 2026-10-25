#pragma once

#include "aegisguard/protocol.hpp"
#include "aegisguard/simulation.hpp"

#include <chrono>

namespace aegisguard {

enum class MovementAnomaly {
    none,
    speed,
    teleport,
    aim_snap,
    invalid_sample,
    clock_regression,
};

struct MovementAnomalyConfig {
    double maximum_speed{5.0};
    double position_latency_grace_seconds{0.25};
    double position_jitter_tolerance{1.0};
    double teleport_distance_threshold{10.0};
    double maximum_aim_rate_radians_per_second{40.0};
    double aim_latency_grace_seconds{0.02};
};

class MovementAnomalyDetector {
public:
    using Clock = std::chrono::steady_clock;

    MovementAnomalyDetector(
        MovementAnomalyConfig config,
        Vec2 initial_position,
        Clock::time_point now);

    [[nodiscard]] MovementAnomaly observe(
        const MoveFrame& frame, Clock::time_point now) noexcept;

private:
    MovementAnomalyConfig config_;
    Vec2 previous_position_;
    double previous_aim_angle_{};
    Clock::time_point previous_time_;
};

void validate_movement_anomaly_config(const MovementAnomalyConfig& config);

}  // namespace aegisguard
