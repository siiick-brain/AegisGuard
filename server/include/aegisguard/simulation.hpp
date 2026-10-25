#pragma once

#include <deque>
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
    double weapon_range{20.0};
    double target_radius{0.5};
    std::uint32_t weapon_damage{25};
    std::uint64_t maximum_rewind_ticks{6};
    std::uint64_t respawn_delay_ticks{180};
};

inline constexpr std::uint32_t kInitialPlayerHealth = 100;

struct PlayerState {
    std::uint64_t id{};
    Vec2 position{};
    Vec2 velocity{};
    std::uint32_t health{kInitialPlayerHealth};
};

struct PlayerInput {
    std::uint64_t player_id{};
    Vec2 movement{};
};

struct FireIntent {
    std::uint64_t player_id{};
    double aim_angle_radians{};
    std::uint64_t tick_reference{};
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
    invalid_fire_intent,
    future_fire_tick,
    stale_fire_tick,
    player_dead,
    spatial_failure,
};

struct FireResult {
    SimulationError error{SimulationError::none};
    std::optional<std::uint64_t> hit_player_id;
};

class Simulation {
public:
    explicit Simulation(SimulationConfig config = {});

    [[nodiscard]] SimulationError add_player(std::uint64_t id, Vec2 position);
    [[nodiscard]] SimulationError step(std::span<const PlayerInput> inputs);
    [[nodiscard]] FireResult fire(const FireIntent& intent);
    [[nodiscard]] std::optional<PlayerState> player(std::uint64_t id) const;
    [[nodiscard]] std::uint64_t tick() const noexcept;
    [[nodiscard]] double fixed_delta_seconds() const noexcept;

private:
    struct Snapshot {
        std::uint64_t tick{};
        std::map<std::uint64_t, Vec2> living_players;
    };

    SimulationConfig config_;
    std::map<std::uint64_t, PlayerState> players_;
    std::map<std::uint64_t, Vec2> spawn_positions_;
    std::map<std::uint64_t, std::uint64_t> respawn_ticks_;
    std::deque<Snapshot> history_;
    std::uint64_t tick_{};

    void record_snapshot();
};

}  // namespace aegisguard
