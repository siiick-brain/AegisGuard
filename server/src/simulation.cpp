#include "aegisguard/simulation.hpp"

#include "aegisguard/quadtree.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
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
        !std::isfinite(config_.max_speed * config_.fixed_delta_seconds) ||
        !finite_positive(config_.weapon_range) ||
        !finite_positive(config_.target_radius) ||
        config_.weapon_damage == 0 ||
        config_.weapon_damage > kInitialPlayerHealth ||
        config_.maximum_rewind_ticks > 120 ||
        config_.respawn_delay_ticks == 0) {
        throw std::invalid_argument("simulation configuration must be finite and positive");
    }
    record_snapshot();
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

    players_.emplace(id, PlayerState{id, position, {}, kInitialPlayerHealth});
    spawn_positions_.emplace(id, position);
    record_snapshot();
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
        if (state.health == 0) {
            state.velocity = {};
            continue;
        }
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
    for (auto respawn = respawn_ticks_.begin();
         respawn != respawn_ticks_.end();) {
        if (respawn->second > tick_) {
            ++respawn;
            continue;
        }
        PlayerState& state = players_.at(respawn->first);
        state.position = spawn_positions_.at(respawn->first);
        state.velocity = {};
        state.health = kInitialPlayerHealth;
        respawn = respawn_ticks_.erase(respawn);
    }
    record_snapshot();
    return SimulationError::none;
}

FireResult Simulation::fire(const FireIntent& intent) {
    const auto shooter = players_.find(intent.player_id);
    if (shooter == players_.end()) {
        return {SimulationError::unknown_player, std::nullopt};
    }
    if (shooter->second.health == 0) {
        return {SimulationError::player_dead, std::nullopt};
    }
    if (!std::isfinite(intent.aim_angle_radians) ||
        intent.aim_angle_radians < 0.0 ||
        intent.aim_angle_radians >= 2.0 * std::numbers::pi) {
        return {SimulationError::invalid_fire_intent, std::nullopt};
    }
    if (intent.tick_reference > tick_) {
        return {SimulationError::future_fire_tick, std::nullopt};
    }
    if (tick_ - intent.tick_reference > config_.maximum_rewind_ticks) {
        return {SimulationError::stale_fire_tick, std::nullopt};
    }

    const auto snapshot = std::find_if(
        history_.begin(), history_.end(), [&intent](const Snapshot& entry) {
            return entry.tick == intent.tick_reference;
        });
    if (snapshot == history_.end()) {
        return {SimulationError::stale_fire_tick, std::nullopt};
    }
    const auto shooter_position = snapshot->living_players.find(intent.player_id);
    if (shooter_position == snapshot->living_players.end()) {
        return {SimulationError::player_dead, std::nullopt};
    }

    Quadtree spatial_index({
        0.0, 0.0, config_.world_width, config_.world_height});
    for (const auto& [player_id, position] : snapshot->living_players) {
        if (spatial_index.insert(player_id, {position.x, position.y}) !=
            SpatialError::none) {
            return {SimulationError::spatial_failure, std::nullopt};
        }
    }

    const Vec2 origin = shooter_position->second;
    const double direction_x = std::cos(intent.aim_angle_radians);
    const double direction_y = std::sin(intent.aim_angle_radians);
    const Vec2 endpoint{
        origin.x + direction_x * config_.weapon_range,
        origin.y + direction_y * config_.weapon_range};
    const auto candidates = spatial_index.query({
        std::min(origin.x, endpoint.x) - config_.target_radius,
        std::min(origin.y, endpoint.y) - config_.target_radius,
        std::max(origin.x, endpoint.x) + config_.target_radius,
        std::max(origin.y, endpoint.y) + config_.target_radius});
    if (candidates.error != SpatialError::none) {
        return {SimulationError::spatial_failure, std::nullopt};
    }

    std::optional<std::uint64_t> hit_player;
    double nearest_distance = std::numeric_limits<double>::infinity();
    for (const std::uint64_t candidate_id : candidates.entity_ids) {
        if (candidate_id == intent.player_id) {
            continue;
        }
        const auto current_target = players_.find(candidate_id);
        const auto historical_target =
            snapshot->living_players.find(candidate_id);
        if (current_target == players_.end() ||
            current_target->second.health == 0 ||
            historical_target == snapshot->living_players.end()) {
            continue;
        }
        const double offset_x = historical_target->second.x - origin.x;
        const double offset_y = historical_target->second.y - origin.y;
        const double along = offset_x * direction_x + offset_y * direction_y;
        const double perpendicular =
            std::abs(offset_x * direction_y - offset_y * direction_x);
        if (along < 0.0 || along > config_.weapon_range ||
            perpendicular > config_.target_radius || along >= nearest_distance) {
            continue;
        }
        nearest_distance = along;
        hit_player = candidate_id;
    }

    if (!hit_player) {
        return {SimulationError::none, std::nullopt};
    }

    PlayerState& target = players_.at(*hit_player);
    if (target.health <= config_.weapon_damage &&
        tick_ > std::numeric_limits<std::uint64_t>::max() -
                    config_.respawn_delay_ticks) {
        return {SimulationError::tick_overflow, std::nullopt};
    }
    if (target.health <= config_.weapon_damage) {
        target.health = 0;
        respawn_ticks_.insert_or_assign(
            *hit_player, tick_ + config_.respawn_delay_ticks);
        record_snapshot();
    } else {
        target.health -= config_.weapon_damage;
    }
    return {SimulationError::none, hit_player};
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

void Simulation::record_snapshot() {
    Snapshot snapshot{tick_, {}};
    for (const auto& [player_id, state] : players_) {
        if (state.health > 0) {
            snapshot.living_players.emplace(player_id, state.position);
        }
    }
    if (!history_.empty() && history_.back().tick == tick_) {
        history_.back() = std::move(snapshot);
    } else {
        history_.push_back(std::move(snapshot));
    }
    const std::size_t maximum_history =
        static_cast<std::size_t>(config_.maximum_rewind_ticks + 1);
    while (history_.size() > maximum_history) {
        history_.pop_front();
    }
}

}  // namespace aegisguard
