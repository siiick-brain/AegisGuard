#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string_view>

namespace aegisguard {

inline constexpr std::size_t kMaximumTrackedRulePlayers = 1024;
inline constexpr std::size_t kMaximumRuleDisconnectThreshold = 128;

enum class RuleSignal {
    movement_out_of_range,
    replayed_sequence,
    movement_rate_exceeded,
    speed_exceeded,
    teleport_distance_exceeded,
    aim_snap_exceeded,
    ip_rate_exceeded,
    movement_intent_queue_full,
};

enum class RecommendedAction {
    observe,
    review,
    throttle,
    disconnect,
};

struct RuleDetectorConfig {
    std::uint64_t window_ticks{120};
    std::size_t review_threshold{2};
    std::size_t throttle_threshold{4};
    std::size_t disconnect_threshold{6};
    std::size_t maximum_tracked_players{1024};
};

struct RuleFinding {
    std::uint32_t schema_version{1};
    std::uint64_t player_id{};
    RuleSignal signal{};
    std::uint64_t tick{};
    std::size_t observations_in_window{};
    std::string_view reason;
    RecommendedAction recommended_action{RecommendedAction::observe};
    RecommendedAction applied_action{RecommendedAction::observe};
    bool other_player_history_evicted{};
};

enum class RuleDetectorError {
    none,
    invalid_player_id,
    tick_regression,
};

struct RuleObservationResult {
    RuleDetectorError error{RuleDetectorError::none};
    std::optional<RuleFinding> finding;
};

[[nodiscard]] std::string_view rule_signal_description(RuleSignal signal) noexcept;

class RuleDetector {
public:
    explicit RuleDetector(RuleDetectorConfig config = {});

    [[nodiscard]] RuleObservationResult observe(
        std::uint64_t player_id, RuleSignal signal, std::uint64_t tick);

private:
    struct PlayerHistory {
        std::uint64_t last_tick{};
        std::map<RuleSignal, std::deque<std::uint64_t>> ticks_by_signal;
        bool history_truncated{};
    };

    void prune(std::uint64_t tick);

    RuleDetectorConfig config_;
    std::map<std::uint64_t, PlayerHistory> histories_;
};

}  // namespace aegisguard
