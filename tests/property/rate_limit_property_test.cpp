#include "aegisguard/protocol.hpp"

#include <gtest/gtest.h>
#include <rapidcheck/gtest.h>

#include <algorithm>
#include <chrono>
#include <vector>

namespace aegisguard {
namespace {

RC_GTEST_PROP(TokenBucketProperty, NeverExceedsBurstPlusElapsedRefill, ()) {
    using Clock = TokenBucket::Clock;
    const int capacity = *rc::gen::inRange(1, 33);
    const int rate = *rc::gen::inRange(1, 121);
    const auto steps = *rc::gen::container<std::vector<int>>(
        rc::gen::inRange(0, 51));
    const auto start = Clock::time_point{};
    TokenBucket bucket(
        static_cast<double>(capacity), static_cast<double>(rate), start);
    auto now = start;
    std::size_t accepted = 0;
    std::chrono::milliseconds elapsed{};

    for (const int step : steps) {
        now += std::chrono::milliseconds(step);
        elapsed += std::chrono::milliseconds(step);
        if (bucket.consume(now)) {
            ++accepted;
        }
        RC_ASSERT(bucket.available_tokens() >= 0.0);
        RC_ASSERT(bucket.available_tokens() <= static_cast<double>(capacity));
    }
    const double maximum_budget = static_cast<double>(capacity) +
        static_cast<double>(rate) *
            std::chrono::duration<double>(elapsed).count();
    RC_ASSERT(static_cast<double>(accepted) <= maximum_budget + 1e-9);
}

}  // namespace
}  // namespace aegisguard
