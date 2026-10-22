#pragma once

#include "aegisguard/simulation.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace aegisguard {

inline constexpr std::size_t kMaximumReplayTicks = 100000;
inline constexpr std::size_t kMaximumReplayPlayers = 1024;
inline constexpr std::size_t kMaximumReplayInputRecords = 1000000;

struct ReplayPlayer {
    std::uint64_t id{};
    Vec2 position{};
};

struct RecordedTick {
    std::uint32_t schema_version{1};
    std::uint64_t tick{};
    std::vector<PlayerInput> inputs;
};

struct ReplaySessionConfig {
    SimulationConfig simulation{};
    std::size_t maximum_ticks{4096};
    std::size_t maximum_players{128};
};

enum class ReplayError {
    none,
    tick_capacity_exceeded,
    input_limit_exceeded,
    simulation_rejected,
    invalid_recording,
};

struct ReplayRecordResult {
    ReplayError error{ReplayError::none};
    SimulationError simulation_error{SimulationError::none};
};

struct ReplayResult {
    ReplayError error{ReplayError::none};
    SimulationError simulation_error{SimulationError::none};
    std::uint64_t final_tick{};
    std::vector<PlayerState> players;
};

class ReplaySession {
public:
    ReplaySession(
        ReplaySessionConfig config,
        std::span<const ReplayPlayer> initial_players);

    [[nodiscard]] ReplayRecordResult record_tick(
        std::span<const PlayerInput> inputs);
    [[nodiscard]] ReplayResult validate_and_replay() const;
    [[nodiscard]] std::uint64_t current_tick() const noexcept;
    [[nodiscard]] std::optional<PlayerState> current_player(
        std::uint64_t id) const;
    [[nodiscard]] std::span<const RecordedTick> recorded_ticks() const noexcept;

private:
    ReplaySessionConfig config_;
    std::vector<ReplayPlayer> initial_players_;
    Simulation simulation_;
    std::vector<RecordedTick> recorded_ticks_;
};

}  // namespace aegisguard
