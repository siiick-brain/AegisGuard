#include "aegisguard/ip_rate_limiter.hpp"

#include <gtest/gtest.h>

#include <stdexcept>

namespace aegisguard {
namespace {

using Clock = IpRateLimiter::Clock;

TEST(IpRateLimiterTest, StateSurvivesReconnectsFromTheSameAddress) {
    const auto start = Clock::time_point{};
    IpRateLimiter limiter(2.0, 0.001, 4);

    EXPECT_TRUE(limiter.consume("127.0.0.1", start));
    EXPECT_TRUE(limiter.consume("127.0.0.1", start));
    EXPECT_FALSE(limiter.consume("127.0.0.1", start + std::chrono::milliseconds(10)));
    EXPECT_FALSE(limiter.consume("127.0.0.1", start + std::chrono::milliseconds(20)));
}

TEST(IpRateLimiterTest, TracksAddressesIndependentlyAndBoundsState) {
    const auto start = Clock::time_point{};
    IpRateLimiter limiter(1.0, 1.0, 1);

    EXPECT_TRUE(limiter.consume("127.0.0.1", start));
    EXPECT_FALSE(limiter.consume("192.0.2.1", start));
    EXPECT_TRUE(limiter.consume(
        "192.0.2.1", start + std::chrono::minutes(6)));
}

TEST(IpRateLimiterTest, RejectsInvalidStateCapacity) {
    EXPECT_THROW(IpRateLimiter limiter(1.0, 1.0, 0), std::invalid_argument);
}

}  // namespace
}  // namespace aegisguard
