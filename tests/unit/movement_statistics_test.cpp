#include "aegisguard/movement_statistics.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

namespace aegisguard {
namespace {

std::vector<RecordedTick> make_trace(std::span<const Vec2> movements) {
    std::vector<RecordedTick> ticks;
    ticks.reserve(movements.size());
    std::uint64_t tick_number = 1;
    for (const Vec2 movement : movements) {
        RecordedTick tick;
        tick.tick = tick_number++;
        tick.inputs.push_back(PlayerInput{17, movement});
        ticks.push_back(std::move(tick));
    }
    return ticks;
}

std::vector<Vec2> repeated_trace() {
    return std::vector<Vec2>(20, Vec2{1.0, 0.0});
}

std::vector<Vec2> varied_trace(std::size_t offset = 0) {
    constexpr std::array movements{
        Vec2{1.0, 0.0},
        Vec2{0.0, 1.0},
        Vec2{-1.0, 0.0},
        Vec2{0.0, -1.0},
    };
    std::vector<Vec2> result;
    result.reserve(20);
    for (std::size_t index = 0; index < 20; ++index) {
        result.push_back(movements[(index + offset) % movements.size()]);
    }
    return result;
}

struct LabeledTrace {
    bool expected_repetitive;
    std::vector<Vec2> movements;
};

LabeledTrace generate_trace(std::uint64_t seed, bool repetitive) {
    std::mt19937_64 random(seed);
    std::vector<Vec2> movements;
    movements.reserve(40);

    if (repetitive) {
        const std::array directions{
            Vec2{1.0, 0.0},
            Vec2{0.0, 1.0},
            Vec2{-1.0, 0.0},
            Vec2{0.0, -1.0},
        };
        const Vec2 direction = directions[random() % directions.size()];
        movements.assign(40, direction);
        return {true, std::move(movements)};
    }

    constexpr std::array directions{
        Vec2{1.0, 0.0},
        Vec2{0.0, 1.0},
        Vec2{-1.0, 0.0},
        Vec2{0.0, -1.0},
    };
    while (movements.size() < 40) {
        const Vec2 direction = directions[random() % directions.size()];
        const std::size_t duration =
            static_cast<std::size_t>(random() % 4) + 1;
        const std::size_t bounded_duration =
            std::min(duration, 40 - movements.size());
        movements.insert(movements.end(), bounded_duration, direction);
    }
    return {false, std::move(movements)};
}

TEST(MovementStatisticsTest, ExtractsVersionedMagnitudeAndRepetitionFeatures) {
    const std::array movements{
        Vec2{1.0, 0.0},
        Vec2{1.0, 0.0},
        Vec2{0.5, 0.0},
    };
    const auto ticks = make_trace(movements);

    const MovementFeatureResult result =
        extract_movement_features(17, ticks);

    ASSERT_EQ(result.error, MovementFeatureError::none);
    EXPECT_EQ(result.features.schema_version, 1U);
    EXPECT_EQ(result.features.player_id, 17U);
    EXPECT_EQ(result.features.sample_count, 3U);
    EXPECT_EQ(result.features.adjacent_pair_count, 2U);
    EXPECT_EQ(result.features.repeated_input_count, 1U);
    EXPECT_DOUBLE_EQ(result.features.mean_magnitude, 5.0 / 6.0);
    EXPECT_NEAR(
        result.features.magnitude_standard_deviation,
        std::sqrt(1.0 / 18.0),
        1e-12);
    EXPECT_DOUBLE_EQ(result.features.repeated_input_fraction, 0.5);
}

TEST(MovementStatisticsTest, DoesNotCompareInputsAcrossMissingTicks) {
    const std::array ticks{
        RecordedTick{1, 1, {{17, {1.0, 0.0}}}},
        RecordedTick{1, 2, {}},
        RecordedTick{1, 3, {{17, {1.0, 0.0}}}},
    };

    const auto result = extract_movement_features(17, ticks);

    ASSERT_EQ(result.error, MovementFeatureError::none);
    EXPECT_EQ(result.features.sample_count, 2U);
    EXPECT_EQ(result.features.adjacent_pair_count, 0U);
    EXPECT_DOUBLE_EQ(result.features.repeated_input_fraction, 0.0);
}

TEST(MovementStatisticsTest, RejectsInvalidIdentityAndMalformedTraces) {
    EXPECT_EQ(
        extract_movement_features(0, {}).error,
        MovementFeatureError::invalid_player_id);

    const std::array wrong_tick{
        RecordedTick{1, 2, {{17, {1.0, 0.0}}}},
    };
    EXPECT_EQ(
        extract_movement_features(17, wrong_tick).error,
        MovementFeatureError::invalid_trace);

    auto duplicate_input = make_trace(std::array{Vec2{1.0, 0.0}});
    duplicate_input[0].inputs.push_back(PlayerInput{17, {0.0, 1.0}});
    EXPECT_EQ(
        extract_movement_features(17, duplicate_input).error,
        MovementFeatureError::invalid_trace);

    auto invalid_movement = make_trace(std::array{Vec2{1.1, 0.0}});
    EXPECT_EQ(
        extract_movement_features(17, invalid_movement).error,
        MovementFeatureError::invalid_trace);
}

TEST(MovementStatisticsTest, RejectsInvalidConfigurationAndFeatures) {
    MovementStatisticsConfig config;
    config.minimum_samples = 0;
    EXPECT_THROW(MovementStatisticsDetector detector(config), std::invalid_argument);

    config = {};
    config.repeated_input_fraction_threshold =
        std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW(MovementStatisticsDetector detector(config), std::invalid_argument);

    MovementStatisticsDetector detector;
    MovementFeatures invalid;
    invalid.player_id = 17;
    invalid.sample_count = 20;
    invalid.adjacent_pair_count = 19;
    invalid.repeated_input_count = 19;
    invalid.repeated_input_fraction = 0.5;
    EXPECT_EQ(
        detector.evaluate(invalid).error,
        MovementStatisticsError::invalid_features);
}

TEST(MovementStatisticsTest, EmitsObserveOnlyFindingAtConfiguredThreshold) {
    MovementStatisticsConfig config;
    config.minimum_samples = 10;
    config.repeated_input_fraction_threshold = 0.8;
    MovementStatisticsDetector detector(config);
    const auto ticks = make_trace(repeated_trace());
    const auto extracted = extract_movement_features(17, ticks);
    ASSERT_EQ(extracted.error, MovementFeatureError::none);

    const auto result = detector.evaluate(extracted.features);

    ASSERT_EQ(result.error, MovementStatisticsError::none);
    ASSERT_TRUE(result.finding);
    EXPECT_EQ(result.finding->schema_version, 1U);
    EXPECT_EQ(result.finding->player_id, 17U);
    EXPECT_DOUBLE_EQ(result.finding->repeated_input_fraction, 1.0);
    EXPECT_DOUBLE_EQ(result.finding->threshold, 0.8);
    EXPECT_EQ(result.finding->applied_action, RecommendedAction::observe);
}

TEST(MovementStatisticsTest, RequiresTheMinimumSampleCountAndThreshold) {
    MovementStatisticsConfig config;
    config.minimum_samples = 20;
    config.repeated_input_fraction_threshold = 0.9;
    MovementStatisticsDetector detector(config);
    const std::array short_trace{
        Vec2{1.0, 0.0},
        Vec2{1.0, 0.0},
    };
    const auto extracted = extract_movement_features(
        17, make_trace(short_trace));
    ASSERT_EQ(extracted.error, MovementFeatureError::none);

    const auto result = detector.evaluate(extracted.features);

    EXPECT_EQ(result.error, MovementStatisticsError::none);
    EXPECT_FALSE(result.finding);
}

TEST(MovementStatisticsEvaluationTest, SeparatesSyntheticTracesFromExpectedLabels) {
    const std::array evaluation_traces{
        LabeledTrace{false, varied_trace()},
        LabeledTrace{false, varied_trace(1)},
        LabeledTrace{true, repeated_trace()},
        LabeledTrace{true, repeated_trace()},
    };
    MovementStatisticsDetector detector;
    std::size_t true_positives = 0;
    std::size_t false_positives = 0;
    std::size_t true_negatives = 0;
    std::size_t false_negatives = 0;

    for (const LabeledTrace& trace : evaluation_traces) {
        const auto ticks = make_trace(trace.movements);
        const auto features = extract_movement_features(17, ticks);
        ASSERT_EQ(features.error, MovementFeatureError::none);
        const auto result = detector.evaluate(features.features);
        ASSERT_EQ(result.error, MovementStatisticsError::none);
        const bool flagged = result.finding.has_value();
        if (trace.expected_repetitive && flagged) {
            ++true_positives;
        } else if (trace.expected_repetitive) {
            ++false_negatives;
        } else if (flagged) {
            ++false_positives;
        } else {
            ++true_negatives;
        }
    }

    EXPECT_EQ(true_positives, 2U);
    EXPECT_EQ(false_positives, 0U);
    EXPECT_EQ(true_negatives, 2U);
    EXPECT_EQ(false_negatives, 0U);
}

TEST(MovementStatisticsEvaluationTest, EvaluatesDisjointSeededHoldout) {
    constexpr std::size_t traces_per_class = 40;
    std::vector<LabeledTrace> development_traces;
    std::vector<LabeledTrace> holdout_traces;
    development_traces.reserve(traces_per_class * 2);
    holdout_traces.reserve(traces_per_class * 2);

    for (std::size_t index = 0; index < traces_per_class; ++index) {
        development_traces.push_back(generate_trace(100 + index, false));
        development_traces.push_back(generate_trace(200 + index, true));
        holdout_traces.push_back(generate_trace(10000 + index, false));
        holdout_traces.push_back(generate_trace(20000 + index, true));
    }

    const auto first_generation = generate_trace(10000, false);
    const auto second_generation = generate_trace(10000, false);
    ASSERT_EQ(
        first_generation.movements.size(),
        second_generation.movements.size());
    EXPECT_TRUE(std::equal(
        first_generation.movements.begin(),
        first_generation.movements.end(),
        second_generation.movements.begin(),
        [](Vec2 left, Vec2 right) {
            return left.x == right.x && left.y == right.y;
        }));

    MovementStatisticsDetector detector;
    std::size_t development_positive_count = 0;
    std::size_t development_negative_count = 0;
    for (const LabeledTrace& trace : development_traces) {
        if (trace.expected_repetitive) {
            ++development_positive_count;
        } else {
            ++development_negative_count;
        }
        EXPECT_EQ(trace.movements.size(), 40U);
    }
    EXPECT_EQ(development_positive_count, traces_per_class);
    EXPECT_EQ(development_negative_count, traces_per_class);

    std::size_t true_positives = 0;
    std::size_t false_positives = 0;
    std::size_t true_negatives = 0;
    std::size_t false_negatives = 0;

    for (const LabeledTrace& trace : holdout_traces) {
        const auto ticks = make_trace(trace.movements);
        const auto features = extract_movement_features(17, ticks);
        ASSERT_EQ(features.error, MovementFeatureError::none);
        const auto result = detector.evaluate(features.features);
        ASSERT_EQ(result.error, MovementStatisticsError::none);
        const bool flagged = result.finding.has_value();
        if (trace.expected_repetitive && flagged) {
            ++true_positives;
        } else if (trace.expected_repetitive) {
            ++false_negatives;
        } else if (flagged) {
            ++false_positives;
        } else {
            ++true_negatives;
        }
    }

    const double precision = static_cast<double>(true_positives) /
        static_cast<double>(true_positives + false_positives);
    const double recall = static_cast<double>(true_positives) /
        static_cast<double>(true_positives + false_negatives);
    const double false_positive_rate = static_cast<double>(false_positives) /
        static_cast<double>(false_positives + true_negatives);
    const double false_negative_rate = static_cast<double>(false_negatives) /
        static_cast<double>(false_negatives + true_positives);

    EXPECT_EQ(true_positives, 40U);
    EXPECT_EQ(false_positives, 0U);
    EXPECT_EQ(true_negatives, 40U);
    EXPECT_EQ(false_negatives, 0U);
    EXPECT_DOUBLE_EQ(precision, 1.0);
    EXPECT_DOUBLE_EQ(recall, 1.0);
    EXPECT_DOUBLE_EQ(false_positive_rate, 0.0);
    EXPECT_DOUBLE_EQ(false_negative_rate, 0.0);
}

}  // namespace
}  // namespace aegisguard
