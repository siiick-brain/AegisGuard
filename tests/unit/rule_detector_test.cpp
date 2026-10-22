#include "aegisguard/rule_detector.hpp"

#include <gtest/gtest.h>

#include <stdexcept>

namespace aegisguard {
namespace {

TEST(RuleDetectorTest, RejectsInvalidConfiguration) {
    RuleDetectorConfig config;
    config.review_threshold = config.throttle_threshold;
    EXPECT_THROW(RuleDetector detector(config), std::invalid_argument);

    config = {};
    config.maximum_tracked_players = kMaximumTrackedRulePlayers + 1;
    EXPECT_THROW(RuleDetector detector(config), std::invalid_argument);

    config = {};
    config.review_threshold = 127;
    config.throttle_threshold = 128;
    config.disconnect_threshold = 129;
    EXPECT_THROW(RuleDetector detector(config), std::invalid_argument);
}

TEST(RuleDetectorTest, EmitsVersionedEvidenceAndKeepsEnforcementInTestMode) {
    RuleDetector detector;

    const auto result = detector.observe(
        17, RuleSignal::movement_out_of_range, 3);

    ASSERT_EQ(result.error, RuleDetectorError::none);
    ASSERT_TRUE(result.finding.has_value());
    EXPECT_EQ(result.finding->schema_version, 1U);
    EXPECT_EQ(result.finding->player_id, 17U);
    EXPECT_EQ(result.finding->signal, RuleSignal::movement_out_of_range);
    EXPECT_EQ(result.finding->observations_in_window, 1U);
    EXPECT_EQ(
        result.finding->reason,
        "Movement components exceeded the normalized input range.");
    EXPECT_EQ(result.finding->recommended_action, RecommendedAction::observe);
    EXPECT_EQ(result.finding->applied_action, RecommendedAction::observe);
}

TEST(RuleDetectorTest, ProgressesRecommendationsForRepeatedSignals) {
    RuleDetector detector;

    const auto first = detector.observe(17, RuleSignal::replayed_sequence, 1);
    const auto second = detector.observe(17, RuleSignal::replayed_sequence, 2);
    const auto third = detector.observe(17, RuleSignal::replayed_sequence, 3);
    const auto fourth = detector.observe(17, RuleSignal::replayed_sequence, 4);
    const auto fifth = detector.observe(17, RuleSignal::replayed_sequence, 5);
    const auto sixth = detector.observe(17, RuleSignal::replayed_sequence, 6);
    const auto seventh = detector.observe(17, RuleSignal::replayed_sequence, 7);

    ASSERT_TRUE(first.finding);
    ASSERT_TRUE(second.finding);
    ASSERT_TRUE(fourth.finding);
    ASSERT_TRUE(sixth.finding);
    ASSERT_TRUE(seventh.finding);
    EXPECT_EQ(first.finding->recommended_action, RecommendedAction::observe);
    EXPECT_EQ(second.finding->recommended_action, RecommendedAction::review);
    EXPECT_EQ(third.finding->recommended_action, RecommendedAction::review);
    EXPECT_EQ(fourth.finding->recommended_action, RecommendedAction::throttle);
    EXPECT_EQ(fifth.finding->recommended_action, RecommendedAction::throttle);
    EXPECT_EQ(sixth.finding->recommended_action, RecommendedAction::disconnect);
    EXPECT_EQ(seventh.finding->observations_in_window, 6U);
    EXPECT_EQ(sixth.finding->applied_action, RecommendedAction::observe);
}

TEST(RuleDetectorTest, CountsRulesIndependentlyAndSeparatesPlayers) {
    RuleDetector detector;

    const auto movement = detector.observe(
        17, RuleSignal::movement_out_of_range, 1);
    const auto replay = detector.observe(17, RuleSignal::replayed_sequence, 2);
    const auto other_player = detector.observe(
        18, RuleSignal::movement_out_of_range, 2);

    ASSERT_TRUE(movement.finding);
    ASSERT_TRUE(replay.finding);
    ASSERT_TRUE(other_player.finding);
    EXPECT_EQ(movement.finding->observations_in_window, 1U);
    EXPECT_EQ(replay.finding->observations_in_window, 1U);
    EXPECT_EQ(other_player.finding->observations_in_window, 1U);
}

TEST(RuleDetectorTest, ExpiresEvidenceOutsideTheConfiguredWindow) {
    RuleDetectorConfig config;
    config.window_ticks = 10;
    RuleDetector detector(config);

    ASSERT_TRUE(detector.observe(
        17, RuleSignal::movement_rate_exceeded, 1).finding);
    const auto result = detector.observe(
        17, RuleSignal::movement_rate_exceeded, 12);

    ASSERT_TRUE(result.finding);
    EXPECT_EQ(result.finding->observations_in_window, 1U);
    EXPECT_EQ(result.finding->recommended_action, RecommendedAction::observe);
}

TEST(RuleDetectorTest, ReportsWhenBoundedHistoryEvictsAPlayer) {
    RuleDetectorConfig config;
    config.maximum_tracked_players = 1;
    RuleDetector detector(config);

    ASSERT_TRUE(detector.observe(
        17, RuleSignal::replayed_sequence, 1).finding);
    const auto result = detector.observe(
        18, RuleSignal::replayed_sequence, 2);

    ASSERT_TRUE(result.finding);
    EXPECT_TRUE(result.finding->other_player_history_evicted);
}

TEST(RuleDetectorTest, RejectsInvalidIdentityAndBackwardsTicks) {
    RuleDetector detector;

    EXPECT_EQ(detector.observe(
                  0, RuleSignal::replayed_sequence, 1).error,
              RuleDetectorError::invalid_player_id);
    ASSERT_TRUE(detector.observe(
        17, RuleSignal::replayed_sequence, 4).finding);
    EXPECT_EQ(detector.observe(
                  17, RuleSignal::replayed_sequence, 3).error,
              RuleDetectorError::tick_regression);
}

TEST(RuleDetectorTest, DescribesEachSupportedSignal) {
    EXPECT_NE(rule_signal_description(RuleSignal::movement_out_of_range).size(), 0U);
    EXPECT_NE(rule_signal_description(RuleSignal::replayed_sequence).size(), 0U);
    EXPECT_NE(rule_signal_description(RuleSignal::movement_rate_exceeded).size(), 0U);
}

}  // namespace
}  // namespace aegisguard
