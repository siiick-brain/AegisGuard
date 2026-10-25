#pragma once

#include "aegisguard/simulation.hpp"

#include <cstddef>
#include <deque>
#include <stdexcept>

namespace aegisguard {

inline constexpr std::size_t kMaximumQueuedIntentsPerPlayer = 1024;

class IntentQueue {
public:
    explicit IntentQueue(std::size_t capacity) : capacity_(capacity) {
        if (capacity_ == 0) {
            throw std::invalid_argument("intent queue capacity must be positive");
        }
    }

    [[nodiscard]] bool enqueue(Vec2 movement) {
        if (pending_.size() >= capacity_) {
            return false;
        }
        pending_.push_back(movement);
        return true;
    }

    [[nodiscard]] Vec2 advance_tick() {
        if (!pending_.empty()) {
            current_ = pending_.front();
            pending_.pop_front();
        }
        return current_;
    }

    [[nodiscard]] std::size_t pending_size() const noexcept {
        return pending_.size();
    }

private:
    std::size_t capacity_;
    std::deque<Vec2> pending_;
    Vec2 current_{};
};

}  // namespace aegisguard
