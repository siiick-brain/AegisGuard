#include "aegisguard/simulation.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace aegisguard {
namespace {

TEST(SimulationTest, RejectsInvalidConfiguration) {
    SimulationConfig config;
    config.max_speed = 0.0;
    EXPECT_THROW(Simulation simulation(config), std::invalid_argument);
}

TEST(SimulationTest, RejectsEachInvalidConfigurationLimit) {
    const auto expect_invalid = [](auto update) {
        SimulationConfig config;
        update(config);
        EXPECT_THROW(Simulation simulation(config), std::invalid_argument);
    };

    expect_invalid([](SimulationConfig& config) {
        config.world_width = 0.0;
    });
    expect_invalid([](SimulationConfig& config) {
        config.world_height = std::numeric_limits<double>::infinity();
    });
    expect_invalid([](SimulationConfig& config) {
        config.fixed_delta_seconds = 0.0;
    });
    expect_invalid([](SimulationConfig& config) {
        config.max_speed = std::numeric_limits<double>::max();
        config.fixed_delta_seconds = 2.0;
    });
    expect_invalid([](SimulationConfig& config) {
        config.weapon_range = 0.0;
    });
    expect_invalid([](SimulationConfig& config) {
        config.target_radius = 0.0;
    });
    expect_invalid([](SimulationConfig& config) {
        config.weapon_damage = 0;
    });
    expect_invalid([](SimulationConfig& config) {
        config.weapon_damage = kInitialPlayerHealth + 1;
    });
    expect_invalid([](SimulationConfig& config) {
        config.maximum_rewind_ticks = 121;
    });
    expect_invalid([](SimulationConfig& config) {
        config.respawn_delay_ticks = 0;
    });
}

TEST(SimulationTest, AddsPlayersInsideWorldBounds) {
    Simulation simulation;
    EXPECT_EQ(simulation.add_player(1, {0.0, 100.0}), SimulationError::none);
    EXPECT_EQ(simulation.add_player(0, {1.0, 1.0}), SimulationError::invalid_player_id);
    EXPECT_EQ(simulation.add_player(1, {1.0, 1.0}), SimulationError::duplicate_player);
    EXPECT_EQ(simulation.add_player(2, {-1.0, 1.0}), SimulationError::invalid_position);
    EXPECT_EQ(simulation.add_player(3, {std::numeric_limits<double>::infinity(), 1.0}),
              SimulationError::invalid_position);
}

TEST(SimulationTest, MovesAtTheConfiguredFixedSpeed) {
    Simulation simulation;
    ASSERT_EQ(simulation.add_player(1, {10.0, 20.0}), SimulationError::none);
    const std::array inputs{PlayerInput{1, {1.0, 0.0}}};

    EXPECT_EQ(simulation.step(inputs), SimulationError::none);

    const auto player = simulation.player(1);
    ASSERT_TRUE(player.has_value());
    EXPECT_DOUBLE_EQ(player->position.x, 10.0 + 5.0 / 60.0);
    EXPECT_DOUBLE_EQ(player->position.y, 20.0);
    EXPECT_DOUBLE_EQ(player->velocity.x, 5.0);
    EXPECT_EQ(simulation.tick(), 1U);
}

TEST(SimulationTest, CapsDiagonalMovementToMaximumSpeed) {
    Simulation simulation;
    ASSERT_EQ(simulation.add_player(1, {10.0, 10.0}), SimulationError::none);
    const std::array inputs{PlayerInput{1, {1.0, 1.0}}};

    ASSERT_EQ(simulation.step(inputs), SimulationError::none);

    const auto player = simulation.player(1);
    ASSERT_TRUE(player.has_value());
    EXPECT_NEAR(std::hypot(player->velocity.x, player->velocity.y), 5.0, 1e-12);
}

TEST(SimulationTest, ClampsMovementToWorldBounds) {
    Simulation simulation;
    ASSERT_EQ(simulation.add_player(1, {99.99, 50.0}), SimulationError::none);
    const std::array inputs{PlayerInput{1, {1.0, 0.0}}};

    ASSERT_EQ(simulation.step(inputs), SimulationError::none);

    const auto player = simulation.player(1);
    ASSERT_TRUE(player.has_value());
    EXPECT_DOUBLE_EQ(player->position.x, 100.0);
}

TEST(SimulationTest, StopsPlayersWithoutInput) {
    Simulation simulation;
    ASSERT_EQ(simulation.add_player(1, {10.0, 10.0}), SimulationError::none);
    const std::array inputs{PlayerInput{1, {1.0, 0.0}}};
    ASSERT_EQ(simulation.step(inputs), SimulationError::none);

    ASSERT_EQ(simulation.step({}), SimulationError::none);

    const auto player = simulation.player(1);
    ASSERT_TRUE(player.has_value());
    EXPECT_DOUBLE_EQ(player->velocity.x, 0.0);
    EXPECT_DOUBLE_EQ(player->position.x, 10.0 + 5.0 / 60.0);
}

TEST(SimulationTest, RejectsUnknownDuplicateAndInvalidInputsWithoutAdvancing) {
    Simulation simulation;
    ASSERT_EQ(simulation.add_player(1, {10.0, 10.0}), SimulationError::none);
    const std::array unknown{PlayerInput{2, {1.0, 0.0}}};
    const std::array duplicate{
        PlayerInput{1, {1.0, 0.0}},
        PlayerInput{1, {0.0, 1.0}},
    };
    const std::array invalid{PlayerInput{1, {2.0, 0.0}}};

    EXPECT_EQ(simulation.step(unknown), SimulationError::unknown_player);
    EXPECT_EQ(simulation.step(duplicate), SimulationError::duplicate_input);
    EXPECT_EQ(simulation.step(invalid), SimulationError::invalid_input);
    EXPECT_EQ(simulation.tick(), 0U);

    const auto player = simulation.player(1);
    ASSERT_TRUE(player.has_value());
    EXPECT_DOUBLE_EQ(player->position.x, 10.0);
    EXPECT_DOUBLE_EQ(player->velocity.x, 0.0);
}

TEST(SimulationTest, RejectsNonFiniteMovement) {
    Simulation simulation;
    ASSERT_EQ(simulation.add_player(1, {10.0, 10.0}), SimulationError::none);
    const std::array inputs{
        PlayerInput{1, {std::numeric_limits<double>::quiet_NaN(), 0.0}},
    };

    EXPECT_EQ(simulation.step(inputs), SimulationError::invalid_input);
    EXPECT_EQ(simulation.tick(), 0U);
}

TEST(SimulationTest, ReturnsNoStateForUnknownPlayers) {
    const Simulation simulation;
    EXPECT_FALSE(simulation.player(42).has_value());
}

TEST(SimulationTest, FireDamagesTheNearestServerOwnedTarget) {
    Simulation simulation;
    ASSERT_EQ(simulation.add_player(1, {10.0, 10.0}), SimulationError::none);
    ASSERT_EQ(simulation.add_player(2, {15.0, 10.0}), SimulationError::none);
    ASSERT_EQ(simulation.add_player(3, {18.0, 10.0}), SimulationError::none);

    const FireResult result = simulation.fire(FireIntent{1, 0.0, 0});

    ASSERT_EQ(result.error, SimulationError::none);
    ASSERT_EQ(result.hit_player_id, 2U);
    EXPECT_EQ(simulation.player(1)->health, 100U);
    EXPECT_EQ(simulation.player(2)->health, 75U);
    EXPECT_EQ(simulation.player(3)->health, 100U);
}

TEST(SimulationTest, FireIncludesTargetsAtMaximumWeaponRange) {
    Simulation simulation;
    ASSERT_EQ(simulation.add_player(1, {10.0, 10.0}), SimulationError::none);
    ASSERT_EQ(simulation.add_player(2, {30.0, 10.0}), SimulationError::none);

    const FireResult result = simulation.fire(FireIntent{1, 0.0, 0});

    ASSERT_EQ(result.error, SimulationError::none);
    EXPECT_EQ(result.hit_player_id, 2U);
    EXPECT_EQ(simulation.player(2)->health, 75U);
}

TEST(SimulationTest, RewindUsesServerRecordedTargetPosition) {
    SimulationConfig config;
    config.maximum_rewind_ticks = 12;
    Simulation simulation(config);
    ASSERT_EQ(simulation.add_player(1, {10.0, 10.0}), SimulationError::none);
    ASSERT_EQ(simulation.add_player(2, {15.0, 10.0}), SimulationError::none);
    const std::array inputs{PlayerInput{2, {0.0, 1.0}}};
    for (int tick = 0; tick < 7; ++tick) {
        ASSERT_EQ(simulation.step(inputs), SimulationError::none);
    }
    ASSERT_GT(simulation.player(2)->position.y, 10.5);

    const FireResult result = simulation.fire(FireIntent{1, 0.0, 0});

    ASSERT_EQ(result.error, SimulationError::none);
    EXPECT_EQ(result.hit_player_id, 2U);
    EXPECT_EQ(simulation.player(2)->health, 75U);
}

TEST(SimulationTest, RejectsFutureAndStaleFireTicksWithoutChangingState) {
    SimulationConfig config;
    config.maximum_rewind_ticks = 2;
    Simulation simulation(config);
    ASSERT_EQ(simulation.add_player(1, {10.0, 10.0}), SimulationError::none);
    ASSERT_EQ(simulation.add_player(2, {15.0, 10.0}), SimulationError::none);

    EXPECT_EQ(
        simulation.fire(FireIntent{1, 0.0, 1}).error,
        SimulationError::future_fire_tick);
    for (int tick = 0; tick < 3; ++tick) {
        ASSERT_EQ(simulation.step({}), SimulationError::none);
    }
    EXPECT_EQ(
        simulation.fire(FireIntent{1, 0.0, 0}).error,
        SimulationError::stale_fire_tick);
    EXPECT_EQ(simulation.player(2)->health, 100U);
}

TEST(SimulationTest, RespawnsEliminatedPlayerAtItsServerOwnedSpawnPoint) {
    SimulationConfig config;
    config.weapon_damage = 100;
    config.respawn_delay_ticks = 2;
    Simulation simulation(config);
    ASSERT_EQ(simulation.add_player(1, {10.0, 10.0}), SimulationError::none);
    ASSERT_EQ(simulation.add_player(2, {15.0, 10.0}), SimulationError::none);

    const FireResult result = simulation.fire(FireIntent{1, 0.0, 0});

    ASSERT_EQ(result.hit_player_id, 2U);
    ASSERT_EQ(simulation.player(2)->health, 0U);
    ASSERT_EQ(simulation.step({}), SimulationError::none);
    EXPECT_EQ(simulation.player(2)->health, 0U);
    ASSERT_EQ(simulation.step({}), SimulationError::none);
    EXPECT_EQ(simulation.player(2)->health, 100U);
    EXPECT_DOUBLE_EQ(simulation.player(2)->position.x, 15.0);
    EXPECT_DOUBLE_EQ(simulation.player(2)->position.y, 10.0);
}

}  // namespace
}  // namespace aegisguard
