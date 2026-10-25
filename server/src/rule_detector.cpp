#include "aegisguard/rule_detector.hpp"

#include <iterator>
#include <stdexcept>

namespace aegisguard {
namespace {

RecommendedAction recommended_action(
    std::size_t observations, const RuleDetectorConfig& config) noexcept {
    if (observations >= config.disconnect_threshold) {
        return RecommendedAction::disconnect;
    }
    if (observations >= config.throttle_threshold) {
        return RecommendedAction::throttle;
    }
    if (observations >= config.review_threshold) {
        return RecommendedAction::review;
    }
    return RecommendedAction::observe;
}

}  // namespace

std::string_view rule_signal_description(RuleSignal signal) noexcept {
    switch (signal) {
        case RuleSignal::movement_out_of_range:
            return "Movement components exceeded the normalized input range.";
        case RuleSignal::replayed_sequence:
            return "A movement frame reused or preceded an accepted sequence.";
        case RuleSignal::movement_rate_exceeded:
            return "Movement frames exceeded the configured token-bucket rate.";
        case RuleSignal::speed_exceeded:
            return "Reported movement exceeded the speed bound with latency tolerance.";
        case RuleSignal::teleport_distance_exceeded:
            return "Reported position exceeded the configured teleport distance.";
        case RuleSignal::aim_snap_exceeded:
            return "Aim angle changed faster than the configured angular rate.";
        case RuleSignal::ip_rate_exceeded:
            return "Movement frames exceeded the shared rate limit for the source IP.";
        case RuleSignal::movement_intent_queue_full:
            return "The bounded per-player movement intent queue is full.";
    }
    return "Unknown rule signal.";
}

RuleDetector::RuleDetector(RuleDetectorConfig config) : config_(config) {
    if (config_.window_ticks == 0 ||
        config_.review_threshold == 0 ||
        config_.review_threshold >= config_.throttle_threshold ||
        config_.throttle_threshold >= config_.disconnect_threshold ||
        config_.disconnect_threshold > kMaximumRuleDisconnectThreshold ||
        config_.maximum_tracked_players == 0 ||
        config_.maximum_tracked_players > kMaximumTrackedRulePlayers) {
        throw std::invalid_argument("rule detector configuration is invalid");
    }
}

RuleObservationResult RuleDetector::observe(
    std::uint64_t player_id, RuleSignal signal, std::uint64_t tick) {
    if (player_id == 0) {
        return {RuleDetectorError::invalid_player_id, std::nullopt};
    }

    prune(tick);
    auto history = histories_.find(player_id);
    if (history != histories_.end() && tick < history->second.last_tick) {
        return {RuleDetectorError::tick_regression, std::nullopt};
    }

    bool truncated = false;
    if (history == histories_.end()) {
        if (histories_.size() >= config_.maximum_tracked_players) {
            auto oldest = histories_.begin();
            for (auto candidate = std::next(histories_.begin());
                 candidate != histories_.end(); ++candidate) {
                if (candidate->second.last_tick < oldest->second.last_tick) {
                    oldest = candidate;
                }
            }
            histories_.erase(oldest);
            truncated = true;
        }
        history = histories_.try_emplace(player_id).first;
    }

    PlayerHistory& player_history = history->second;
    player_history.last_tick = tick;
    player_history.history_truncated =
        player_history.history_truncated || truncated;

    auto& signal_ticks = player_history.ticks_by_signal[signal];
    signal_ticks.push_back(tick);
    if (signal_ticks.size() > config_.disconnect_threshold) {
        signal_ticks.pop_front();
    }
    const std::size_t count = signal_ticks.size();
    const bool other_player_history_evicted = player_history.history_truncated;
    player_history.history_truncated = false;

    return {
        RuleDetectorError::none,
        RuleFinding{
            1,
            player_id,
            signal,
            tick,
            count,
            rule_signal_description(signal),
            recommended_action(count, config_),
            RecommendedAction::observe,
            other_player_history_evicted,
        },
    };
}

void RuleDetector::prune(std::uint64_t tick) {
    for (auto player = histories_.begin(); player != histories_.end();) {
        for (auto signal = player->second.ticks_by_signal.begin();
             signal != player->second.ticks_by_signal.end();) {
            auto& signal_ticks = signal->second;
            while (!signal_ticks.empty() &&
                   tick >= signal_ticks.front() &&
                   tick - signal_ticks.front() > config_.window_ticks) {
                signal_ticks.pop_front();
            }
            if (signal_ticks.empty()) {
                signal = player->second.ticks_by_signal.erase(signal);
            } else {
                ++signal;
            }
        }
        if (player->second.ticks_by_signal.empty()) {
            player = histories_.erase(player);
        } else {
            ++player;
        }
    }
}

}  // namespace aegisguard
