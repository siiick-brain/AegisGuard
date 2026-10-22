#include "aegisguard/simulation.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace aegisguard {
namespace {

bool finite_positive(double value) {
    return std::isfinite(value) && value > 0.0;
}

bool valid_position(Vec2 position, const SimulationConfig& config) {
    return std::isfinite(position.x) && std::isfinite(position.y) &&
           position.x >= 0.0 && position.x <= config.world_width &&
           position.y >= 0.0 && position.y <= config.world_height;
}

}  // namespace

Simulation::Simulation(SimulationConfig config) : config_(config) {
    if (!finite_positive(config_.world_width) ||
        !finite_positive(config_.world_height) ||
        !finite_positive(config_.max_speed) ||
        !finite_positive(config_.fixed_delta_seconds) ||
        !std::isfinite(config_.max_speed * config_.fixed_delta_seconds)) {
        throw std::invalid_argument("simulation configuration must be finite and positive");
    }
}

SimulationError Simulation::add_player(std::uint64_t id, Vec2 position) {
    if (id == 0) {
        return SimulationError::invalid_player_id;
    }
    if (players_.contains(id)) {
        return SimulationError::duplicate_player;
    }
    if (!valid_position(position, config_)) {
        return SimulationError::invalid_position;
    }

    players_.emplace(id, PlayerState{id, position, {}, 100});
    return SimulationError::none;
}

SimulationError Simulation::step(std::span<const PlayerInput> inputs) {
    if (tick_ == std::numeric_limits<std::uint64_t>::max()) {
        return SimulationError::tick_overflow;
    }

    std::map<std::uint64_t, Vec2> movement_by_player;
    for (const PlayerInput& input : inputs) {
        if (!players_.contains(input.player_id)) {
            return SimulationError::unknown_player;
        }
        if (movement_by_player.contains(input.player_id)) {
            return SimulationError::duplicate_input;
        }
        if (!std::isfinite(input.movement.x) || !std::isfinite(input.movement.y) ||
            std::abs(input.movement.x) > 1.0 || std::abs(input.movement.y) > 1.0) {
            return SimulationError::invalid_input;
        }
        movement_by_player.emplace(input.player_id, input.movement);
    }

    for (auto& [id, state] : players_) {
        const auto movement = movement_by_player.find(id);
        Vec2 velocity{};
        if (movement != movement_by_player.end()) {
            const double magnitude = std::hypot(movement->second.x, movement->second.y);
            const double scale = magnitude > 1.0 ? 1.0 / magnitude : 1.0;
            velocity = {
                movement->second.x * scale * config_.max_speed,
                movement->second.y * scale * config_.max_speed,
            };
        }

        state.velocity = velocity;
        state.position.x = std::clamp(
            state.position.x + velocity.x * config_.fixed_delta_seconds,
            0.0,
            config_.world_width);
        state.position.y = std::clamp(
            state.position.y + velocity.y * config_.fixed_delta_seconds,
            0.0,
            config_.world_height);
    }

    ++tick_;
    return SimulationError::none;
}

std::optional<PlayerState> Simulation::player(std::uint64_t id) const {
    const auto found = players_.find(id);
    if (found == players_.end()) {
        return std::nullopt;
    }
    return found->second;
}

std::uint64_t Simulation::tick() const noexcept {
    return tick_;
}

double Simulation::fixed_delta_seconds() const noexcept {
    return config_.fixed_delta_seconds;
}

}  // namespace aegisguard
