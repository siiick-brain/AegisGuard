#pragma once

#include "aegisguard/protocol.hpp"

#include <chrono>
#include <cstddef>
#include <map>
#include <string>
#include <string_view>

namespace aegisguard {

inline constexpr std::size_t kMaximumTrackedIpAddresses = 65536;

class IpRateLimiter {
public:
    using Clock = TokenBucket::Clock;

    IpRateLimiter(
        double capacity,
        double tokens_per_second,
        std::size_t maximum_tracked_addresses);

    [[nodiscard]] bool consume(
        std::string_view address, Clock::time_point now);

private:
    struct State {
        explicit State(double capacity, double rate, Clock::time_point now)
            : bucket(capacity, rate, now), last_seen(now) {}

        TokenBucket bucket;
        Clock::time_point last_seen;
    };

    static constexpr auto kStateLifetime = std::chrono::minutes{5};

    double capacity_;
    double tokens_per_second_;
    std::size_t maximum_tracked_addresses_;
    std::map<std::string, State> states_;
};

}  // namespace aegisguard
