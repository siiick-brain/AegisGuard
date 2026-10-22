#include "aegisguard/replay_session.hpp"

#include <stdexcept>
#include <utility>

namespace aegisguard {

ReplaySession::ReplaySession(
    ReplaySessionConfig config,
    std::span<const ReplayPlayer> initial_players)
    : config_(config),
      simulation_(config_.simulation) {
    if (config_.maximum_ticks == 0 ||
        config_.maximum_ticks > kMaximumReplayTicks ||
        config_.maximum_players == 0 ||
        config_.maximum_players > kMaximumReplayPlayers ||
        initial_players.empty() ||
        initial_players.size() > config_.maximum_players ||
        config_.maximum_ticks >
            kMaximumReplayInputRecords / initial_players.size()) {
        throw std::invalid_argument("replay session limits are invalid");
    }

    initial_players_.assign(initial_players.begin(), initial_players.end());
    recorded_ticks_.reserve(config_.maximum_ticks);
    for (const ReplayPlayer& player : initial_players_) {
        if (simulation_.add_player(player.id, player.position) !=
            SimulationError::none) {
            throw std::invalid_argument("replay session initial player is invalid");
        }
    }
}

ReplayRecordResult ReplaySession::record_tick(
    std::span<const PlayerInput> inputs) {
    if (recorded_ticks_.size() >= config_.maximum_ticks) {
        return {ReplayError::tick_capacity_exceeded, SimulationError::none};
    }
    if (inputs.size() > initial_players_.size()) {
        return {ReplayError::input_limit_exceeded, SimulationError::none};
    }

    RecordedTick recorded_tick;
    recorded_tick.tick = simulation_.tick() + 1;
    recorded_tick.inputs.assign(inputs.begin(), inputs.end());

    const SimulationError step_error = simulation_.step(inputs);
    if (step_error != SimulationError::none) {
        return {ReplayError::simulation_rejected, step_error};
    }

    recorded_ticks_.push_back(std::move(recorded_tick));
    return {};
}

ReplayResult ReplaySession::validate_and_replay() const {
    Simulation replay(config_.simulation);
    for (const ReplayPlayer& player : initial_players_) {
        if (replay.add_player(player.id, player.position) !=
            SimulationError::none) {
            return {
                ReplayError::invalid_recording,
                SimulationError::none,
                0,
                {},
            };
        }
    }

    for (const RecordedTick& recorded_tick : recorded_ticks_) {
        if (recorded_tick.schema_version != 1 ||
            recorded_tick.tick != replay.tick() + 1 ||
            recorded_tick.inputs.size() > initial_players_.size()) {
            return {
                ReplayError::invalid_recording,
                SimulationError::none,
                0,
                {},
            };
        }
        const SimulationError step_error = replay.step(recorded_tick.inputs);
        if (step_error != SimulationError::none) {
            return {
                ReplayError::invalid_recording,
                step_error,
                0,
                {},
            };
        }
    }

    ReplayResult result;
    result.final_tick = replay.tick();
    result.players.reserve(initial_players_.size());
    for (const ReplayPlayer& initial_player : initial_players_) {
        const auto player = replay.player(initial_player.id);
        if (!player) {
            return {
                ReplayError::invalid_recording,
                SimulationError::none,
                0,
                {},
            };
        }
        result.players.push_back(*player);
    }
    return result;
}

std::uint64_t ReplaySession::current_tick() const noexcept {
    return simulation_.tick();
}

std::optional<PlayerState> ReplaySession::current_player(
    std::uint64_t id) const {
    return simulation_.player(id);
}

std::span<const RecordedTick> ReplaySession::recorded_ticks() const noexcept {
    return recorded_ticks_;
}

}  // namespace aegisguard
