#include "aegisguard/movement_anomaly.hpp"

#include <cmath>
#include <numbers>
#include <stdexcept>

namespace aegisguard {

void validate_movement_anomaly_config(const MovementAnomalyConfig& config) {
    if (!std::isfinite(config.maximum_speed) ||
        config.maximum_speed <= 0.0 ||
        !std::isfinite(config.position_latency_grace_seconds) ||
        config.position_latency_grace_seconds < 0.0 ||
        !std::isfinite(config.position_jitter_tolerance) ||
        config.position_jitter_tolerance < 0.0 ||
        !std::isfinite(config.teleport_distance_threshold) ||
        config.teleport_distance_threshold <=
            config.position_jitter_tolerance ||
        !std::isfinite(config.maximum_aim_rate_radians_per_second) ||
        config.maximum_aim_rate_radians_per_second <= 0.0 ||
        !std::isfinite(config.aim_latency_grace_seconds) ||
        config.aim_latency_grace_seconds < 0.0) {
        throw std::invalid_argument("movement anomaly configuration is invalid");
    }
}

MovementAnomalyDetector::MovementAnomalyDetector(
    MovementAnomalyConfig config,
    Vec2 initial_position,
    Clock::time_point now)
    : config_(config),
      previous_position_(initial_position),
      previous_time_(now) {
    validate_movement_anomaly_config(config_);
    if (!std::isfinite(initial_position.x) ||
        !std::isfinite(initial_position.y)) {
        throw std::invalid_argument("movement anomaly initial position is invalid");
    }
}

MovementAnomaly MovementAnomalyDetector::observe(
    const MoveFrame& frame, Clock::time_point now) noexcept {
    if (!std::isfinite(frame.reported_position_x) ||
        !std::isfinite(frame.reported_position_y) ||
        !std::isfinite(frame.aim_angle_radians)) {
        return MovementAnomaly::invalid_sample;
    }
    if (now < previous_time_) {
        return MovementAnomaly::clock_regression;
    }
    const double elapsed = std::chrono::duration<double>(
        now - previous_time_).count();
    const double distance = std::hypot(
        frame.reported_position_x - previous_position_.x,
        frame.reported_position_y - previous_position_.y);
    if (distance > config_.teleport_distance_threshold) {
        return MovementAnomaly::teleport;
    }
    const double maximum_distance = config_.maximum_speed *
        (elapsed + config_.position_latency_grace_seconds) +
        config_.position_jitter_tolerance;
    if (distance > maximum_distance) {
        return MovementAnomaly::speed;
    }

    const double aim_delta = std::abs(std::remainder(
        frame.aim_angle_radians - previous_aim_angle_,
        2.0 * std::numbers::pi));
    const double maximum_aim_delta =
        config_.maximum_aim_rate_radians_per_second *
        (elapsed + config_.aim_latency_grace_seconds);
    if (aim_delta > maximum_aim_delta) {
        return MovementAnomaly::aim_snap;
    }

    previous_position_ = {
        frame.reported_position_x,
        frame.reported_position_y,
    };
    previous_aim_angle_ = frame.aim_angle_radians;
    previous_time_ = now;
    return MovementAnomaly::none;
}

}  // namespace aegisguard
