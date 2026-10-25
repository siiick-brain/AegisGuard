#include "aegisguard/ip_rate_limiter.hpp"

#include <stdexcept>

namespace aegisguard {

IpRateLimiter::IpRateLimiter(
    double capacity,
    double tokens_per_second,
    std::size_t maximum_tracked_addresses)
    : capacity_(capacity),
      tokens_per_second_(tokens_per_second),
      maximum_tracked_addresses_(maximum_tracked_addresses) {
    if (maximum_tracked_addresses_ == 0) {
        throw std::invalid_argument("IP rate state capacity must be positive");
    }
    static_cast<void>(TokenBucket(capacity_, tokens_per_second_, Clock::now()));
}

bool IpRateLimiter::consume(std::string_view address, Clock::time_point now) {
    if (address.empty()) {
        return false;
    }
    for (auto state = states_.begin(); state != states_.end();) {
        if (now >= state->second.last_seen &&
            now - state->second.last_seen > kStateLifetime) {
            state = states_.erase(state);
        } else {
            ++state;
        }
    }

    auto state = states_.find(std::string(address));
    if (state == states_.end()) {
        if (states_.size() >= maximum_tracked_addresses_) {
            return false;
        }
        state = states_.try_emplace(
            std::string(address), capacity_, tokens_per_second_, now).first;
    }
    state->second.last_seen = now;
    return state->second.bucket.consume(now);
}

}  // namespace aegisguard
