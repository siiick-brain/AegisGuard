#include "aegisguard/movement_statistics.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace aegisguard {
namespace {

constexpr std::string_view kRepeatedMovementReason =
    "Movement inputs repeated across consecutive recorded ticks.";

bool valid_features(const MovementFeatures& features) {
    if (features.schema_version != kMovementFeatureSchemaVersion ||
        features.player_id == 0 ||
        features.repeated_input_count > features.adjacent_pair_count ||
        (features.sample_count == 0
             ? features.adjacent_pair_count != 0
             : features.adjacent_pair_count >= features.sample_count) ||
        !std::isfinite(features.mean_magnitude) ||
        features.mean_magnitude < 0.0 ||
        features.mean_magnitude > 1.0 ||
        !std::isfinite(features.magnitude_standard_deviation) ||
        features.magnitude_standard_deviation < 0.0 ||
        !std::isfinite(features.repeated_input_fraction) ||
        features.repeated_input_fraction < 0.0 ||
        features.repeated_input_fraction > 1.0) {
        return false;
    }

    const double expected_fraction = features.adjacent_pair_count == 0
        ? 0.0
        : static_cast<double>(features.repeated_input_count) /
              static_cast<double>(features.adjacent_pair_count);
    return std::abs(features.repeated_input_fraction - expected_fraction) <=
           1e-12;
}

}  // namespace

MovementFeatureResult extract_movement_features(
    std::uint64_t player_id,
    std::span<const RecordedTick> ticks) {
    if (player_id == 0) {
        return {MovementFeatureError::invalid_player_id, {}};
    }

    MovementFeatures features;
    features.player_id = player_id;
    std::uint64_t expected_tick = 1;
    bool previous_tick_had_input = false;
    Vec2 previous_movement{};
    double magnitude_mean = 0.0;
    double magnitude_squared_deviations = 0.0;

    for (const RecordedTick& tick : ticks) {
        if (tick.schema_version != 1 || tick.tick != expected_tick) {
            return {MovementFeatureError::invalid_trace, {}};
        }

        const PlayerInput* player_input = nullptr;
        for (const PlayerInput& input : tick.inputs) {
            if (input.player_id != player_id) {
                continue;
            }
            if (player_input != nullptr ||
                !std::isfinite(input.movement.x) ||
                !std::isfinite(input.movement.y) ||
                std::abs(input.movement.x) > 1.0 ||
                std::abs(input.movement.y) > 1.0) {
                return {MovementFeatureError::invalid_trace, {}};
            }
            player_input = &input;
        }

        if (player_input != nullptr) {
            const Vec2 movement = player_input->movement;
            const double magnitude = std::min(1.0, std::hypot(movement.x, movement.y));
            ++features.sample_count;
            const double delta = magnitude - magnitude_mean;
            magnitude_mean += delta / static_cast<double>(features.sample_count);
            magnitude_squared_deviations +=
                delta * (magnitude - magnitude_mean);

            if (previous_tick_had_input) {
                ++features.adjacent_pair_count;
                if (movement.x == previous_movement.x &&
                    movement.y == previous_movement.y) {
                    ++features.repeated_input_count;
                }
            }
            previous_movement = movement;
        }

        previous_tick_had_input = player_input != nullptr;
        if (expected_tick == UINT64_MAX && &tick != &ticks.back()) {
            return {MovementFeatureError::invalid_trace, {}};
        }
        ++expected_tick;
    }

    features.mean_magnitude = magnitude_mean;
    if (features.sample_count > 0) {
        features.magnitude_standard_deviation = std::sqrt(
            magnitude_squared_deviations /
            static_cast<double>(features.sample_count));
    }
    if (features.adjacent_pair_count > 0) {
        features.repeated_input_fraction =
            static_cast<double>(features.repeated_input_count) /
            static_cast<double>(features.adjacent_pair_count);
    }
    return {MovementFeatureError::none, features};
}

MovementStatisticsDetector::MovementStatisticsDetector(
    MovementStatisticsConfig config)
    : config_(config) {
    if (config_.minimum_samples == 0 ||
        !std::isfinite(config_.repeated_input_fraction_threshold) ||
        config_.repeated_input_fraction_threshold <= 0.0 ||
        config_.repeated_input_fraction_threshold > 1.0) {
        throw std::invalid_argument(
            "movement statistics configuration is invalid");
    }
}

MovementStatisticsResult MovementStatisticsDetector::evaluate(
    const MovementFeatures& features) const {
    if (!valid_features(features)) {
        return {MovementStatisticsError::invalid_features, std::nullopt};
    }
    if (features.sample_count < config_.minimum_samples ||
        features.repeated_input_fraction <
            config_.repeated_input_fraction_threshold) {
        return {};
    }

    StatisticalFinding finding;
    finding.player_id = features.player_id;
    finding.sample_count = features.sample_count;
    finding.repeated_input_fraction = features.repeated_input_fraction;
    finding.threshold = config_.repeated_input_fraction_threshold;
    finding.reason = kRepeatedMovementReason;
    return {MovementStatisticsError::none, finding};
}

}  // namespace aegisguard
