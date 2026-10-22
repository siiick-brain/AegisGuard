#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <span>

namespace aegisguard {

struct Vec2 {
    double x{};
    double y{};
};

struct SimulationConfig {
    double world_width{100.0};
    double world_height{100.0};
    double max_speed{5.0};
    double fixed_delta_seconds{1.0 / 60.0};
};

struct PlayerState {
    std::uint64_t id{};
    Vec2 position{};
    Vec2 velocity{};
    std::uint32_t health{100};
};

struct PlayerInput {
    std::uint64_t player_id{};
    Vec2 movement{};
};

enum class SimulationError {
    none,
    invalid_configuration,
    invalid_player_id,
    duplicate_player,
    invalid_position,
    unknown_player,
    duplicate_input,
    invalid_input,
    tick_overflow,
};

class Simulation {
public:
    explicit Simulation(SimulationConfig config = {});

    [[nodiscard]] SimulationError add_player(std::uint64_t id, Vec2 position);
    [[nodiscard]] SimulationError step(std::span<const PlayerInput> inputs);
    [[nodiscard]] std::optional<PlayerState> player(std::uint64_t id) const;
    [[nodiscard]] std::uint64_t tick() const noexcept;
    [[nodiscard]] double fixed_delta_seconds() const noexcept;

private:
    SimulationConfig config_;
    std::map<std::uint64_t, PlayerState> players_;
    std::uint64_t tick_{};
};

}  // namespace aegisguard
