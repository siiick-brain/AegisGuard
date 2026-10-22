#pragma once

#include "aegisguard/replay_session.hpp"
#include "aegisguard/rule_detector.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace aegisguard {

inline constexpr std::uint32_t kMovementFeatureSchemaVersion = 1;

struct MovementFeatures {
    std::uint32_t schema_version{kMovementFeatureSchemaVersion};
    std::uint64_t player_id{};
    std::size_t sample_count{};
    std::size_t adjacent_pair_count{};
    std::size_t repeated_input_count{};
    double mean_magnitude{};
    double magnitude_standard_deviation{};
    double repeated_input_fraction{};
};

enum class MovementFeatureError {
    none,
    invalid_player_id,
    invalid_trace,
};

struct MovementFeatureResult {
    MovementFeatureError error{MovementFeatureError::none};
    MovementFeatures features{};
};

[[nodiscard]] MovementFeatureResult extract_movement_features(
    std::uint64_t player_id,
    std::span<const RecordedTick> ticks);

struct MovementStatisticsConfig {
    std::size_t minimum_samples{20};
    double repeated_input_fraction_threshold{0.9};
};

struct StatisticalFinding {
    std::uint32_t schema_version{1};
    std::uint64_t player_id{};
    std::size_t sample_count{};
    double repeated_input_fraction{};
    double threshold{};
    std::string_view reason;
    RecommendedAction applied_action{RecommendedAction::observe};
};

enum class MovementStatisticsError {
    none,
    invalid_features,
};

struct MovementStatisticsResult {
    MovementStatisticsError error{MovementStatisticsError::none};
    std::optional<StatisticalFinding> finding;
};

class MovementStatisticsDetector {
public:
    explicit MovementStatisticsDetector(
        MovementStatisticsConfig config = {});

    [[nodiscard]] MovementStatisticsResult evaluate(
        const MovementFeatures& features) const;

private:
    MovementStatisticsConfig config_;
};

}  // namespace aegisguard
