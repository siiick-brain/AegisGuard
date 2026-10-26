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

TEST(MovementAnomalyTest, AcceptsExactSpeedTeleportAndAimThresholds) {
    const auto start = Clock::time_point{};
    MovementAnomalyConfig config;
    config.maximum_speed = 5.0;
    config.position_latency_grace_seconds = 0.0;
    config.position_jitter_tolerance = 0.0;
    config.teleport_distance_threshold = 10.0;
    config.maximum_aim_rate_radians_per_second = 1.0;
    config.aim_latency_grace_seconds = 0.0;

    MovementAnomalyDetector speed_detector(config, Vec2{0.0, 0.0}, start);
    EXPECT_EQ(speed_detector.observe(
                  sample(5.0, 0.0), start + std::chrono::seconds(1)),
              MovementAnomaly::none);

    config.maximum_speed = 100.0;
    MovementAnomalyDetector teleport_detector(
        config, Vec2{0.0, 0.0}, start);
    EXPECT_EQ(teleport_detector.observe(
                  sample(10.0, 0.0), start + std::chrono::seconds(1)),
              MovementAnomaly::none);

    config.teleport_distance_threshold = 100.0;
    MovementAnomalyDetector aim_detector(config, Vec2{0.0, 0.0}, start);
    EXPECT_EQ(aim_detector.observe(
                  sample(0.0, 0.0, 1.0), start + std::chrono::seconds(1)),
              MovementAnomaly::none);
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

TEST(MovementAnomalyTest, AcceptsAimWithinConfiguredLatencyGrace) {
    const auto start = Clock::time_point{};
    MovementAnomalyConfig config;
    config.maximum_aim_rate_radians_per_second = 10.0;
    config.aim_latency_grace_seconds = 0.05;
    MovementAnomalyDetector detector(config, Vec2{0.0, 0.0}, start);

    EXPECT_EQ(detector.observe(
                  sample(0.0, 0.0, 0.4),
                  start + std::chrono::milliseconds(10)),
              MovementAnomaly::none);
}

TEST(MovementAnomalyTest, RejectsNonFiniteReportedTelemetry) {
    const auto start = Clock::time_point{};
    MovementAnomalyDetector detector({}, Vec2{0.0, 0.0}, start);
    EXPECT_EQ(detector.observe(
                  sample(std::numeric_limits<double>::infinity(), 0.0), start),
              MovementAnomaly::invalid_sample);
    EXPECT_EQ(detector.observe(
                  sample(0.0, std::numeric_limits<double>::infinity()), start),
              MovementAnomaly::invalid_sample);
    EXPECT_EQ(detector.observe(
                  sample(0.0, 0.0, std::numeric_limits<double>::infinity()),
                  start),
              MovementAnomaly::invalid_sample);
}

TEST(MovementAnomalyTest, RejectsClockRegression) {
    const auto start = Clock::time_point{};
    MovementAnomalyDetector detector({}, Vec2{0.0, 0.0}, start);

    EXPECT_EQ(detector.observe(sample(0.0, 0.0), start - std::chrono::milliseconds(1)),
              MovementAnomaly::clock_regression);
}

TEST(MovementAnomalyTest, RejectsInvalidThresholdConfiguration) {
    const auto expect_invalid = [](auto update) {
        MovementAnomalyConfig config;
        update(config);
        EXPECT_THROW(
            MovementAnomalyDetector(
                config, Vec2{0.0, 0.0}, Clock::time_point{}),
            std::invalid_argument);
    };

    expect_invalid([](MovementAnomalyConfig& config) {
        config.maximum_speed = 0.0;
    });
    expect_invalid([](MovementAnomalyConfig& config) {
        config.maximum_speed = std::numeric_limits<double>::quiet_NaN();
    });
    expect_invalid([](MovementAnomalyConfig& config) {
        config.position_latency_grace_seconds = -1.0;
    });
    expect_invalid([](MovementAnomalyConfig& config) {
        config.position_latency_grace_seconds =
            std::numeric_limits<double>::infinity();
    });
    expect_invalid([](MovementAnomalyConfig& config) {
        config.position_jitter_tolerance = -1.0;
    });
    expect_invalid([](MovementAnomalyConfig& config) {
        config.position_jitter_tolerance =
            std::numeric_limits<double>::infinity();
    });
    expect_invalid([](MovementAnomalyConfig& config) {
        config.teleport_distance_threshold =
            std::numeric_limits<double>::infinity();
    });
    expect_invalid([](MovementAnomalyConfig& config) {
        config.teleport_distance_threshold = 0.5;
        config.position_jitter_tolerance = 1.0;
    });
    expect_invalid([](MovementAnomalyConfig& config) {
        config.maximum_aim_rate_radians_per_second = 0.0;
    });
    expect_invalid([](MovementAnomalyConfig& config) {
        config.maximum_aim_rate_radians_per_second =
            std::numeric_limits<double>::infinity();
    });
    expect_invalid([](MovementAnomalyConfig& config) {
        config.aim_latency_grace_seconds = -1.0;
    });
    expect_invalid([](MovementAnomalyConfig& config) {
        config.aim_latency_grace_seconds =
            std::numeric_limits<double>::quiet_NaN();
    });
}

}  // namespace
}  // namespace aegisguard
