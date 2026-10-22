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

}  // namespace
}  // namespace aegisguard
