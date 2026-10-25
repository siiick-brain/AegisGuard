#include "aegisguard/movement_anomaly.hpp"

#include <gtest/gtest.h>

#include <limits>
#include <numbers>
#include <stdexcept>

namespace aegisguard {
namespace {

using Clock = MovementAnomalyDetector::Clock;

MoveFrame sample(double x, double y, double aim = 0.0) {
    MoveFrame frame;
    frame.reported_position_x = x;
    frame.reported_position_y = y;
    frame.aim_angle_radians = aim;
    return frame;
}

TEST(MovementAnomalyTest, SpeedCheatRejected) {
    const auto start = Clock::time_point{};
    MovementAnomalyDetector detector({}, Vec2{0.0, 0.0}, start);

    EXPECT_EQ(detector.observe(sample(9.0, 0.0), start + std::chrono::milliseconds(100)),
              MovementAnomaly::speed);
}

TEST(MovementAnomalyTest, TeleportCheatRejected) {
    const auto start = Clock::time_point{};
    MovementAnomalyDetector detector({}, Vec2{0.0, 0.0}, start);

    EXPECT_EQ(detector.observe(sample(10.1, 0.0), start),
              MovementAnomaly::teleport);
}

TEST(MovementAnomalyTest, AimSnapCheatRejected) {
    const auto start = Clock::time_point{};
    MovementAnomalyDetector detector({}, Vec2{0.0, 0.0}, start);

    EXPECT_EQ(detector.observe(
                  sample(0.0, 0.0, std::numbers::pi),
                  start + std::chrono::milliseconds(10)),
              MovementAnomaly::aim_snap);
}

TEST(MovementAnomalyTest, FalsePositiveLatency) {
    const auto start = Clock::time_point{};
    MovementAnomalyDetector detector({}, Vec2{0.0, 0.0}, start);

    EXPECT_EQ(detector.observe(sample(5.0, 0.0),
                               start + std::chrono::seconds(1)),
              MovementAnomaly::none);
}

TEST(MovementAnomalyTest, FalsePositiveJitter) {
    const auto start = Clock::time_point{};
    MovementAnomalyDetector detector({}, Vec2{10.0, 10.0}, start);

    EXPECT_EQ(detector.observe(sample(10.35, 9.8, 0.01),
                               start + std::chrono::milliseconds(16)),
              MovementAnomaly::none);
    EXPECT_EQ(detector.observe(sample(9.9, 10.25, 0.02),
                               start + std::chrono::milliseconds(32)),
              MovementAnomaly::none);
}

TEST(MovementAnomalyTest, FalsePositiveFastLegitimateTurn) {
    const auto start = Clock::time_point{};
    MovementAnomalyDetector detector({}, Vec2{0.0, 0.0}, start);

    EXPECT_EQ(detector.observe(
                  sample(0.0, 0.0, std::numbers::pi),
                  start + std::chrono::milliseconds(100)),
              MovementAnomaly::none);
}

TEST(MovementAnomalyTest, RejectsNonFiniteReportedTelemetry) {
    const auto start = Clock::time_point{};
    MovementAnomalyDetector detector({}, Vec2{0.0, 0.0}, start);
    const MoveFrame invalid = sample(
        std::numeric_limits<double>::infinity(), 0.0);

    EXPECT_EQ(detector.observe(invalid, start), MovementAnomaly::invalid_sample);
}

TEST(MovementAnomalyTest, RejectsInvalidThresholdConfiguration) {
    MovementAnomalyConfig config;
    config.teleport_distance_threshold = 0.5;
    config.position_jitter_tolerance = 1.0;

    EXPECT_THROW(
        MovementAnomalyDetector(config, Vec2{0.0, 0.0}, Clock::time_point{}),
        std::invalid_argument);
}

}  // namespace
}  // namespace aegisguard
