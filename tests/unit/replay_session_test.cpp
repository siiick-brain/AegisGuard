#include "aegisguard/replay_session.hpp"

#include <gtest/gtest.h>

#include <array>
#include <stdexcept>

namespace aegisguard {
namespace {

TEST(ReplaySessionTest, RecordsAndReplaysValidatedSimulationInputs) {
    const std::array players{
        ReplayPlayer{1, {10.0, 10.0}},
        ReplayPlayer{2, {20.0, 20.0}},
    };
    ReplaySession session({}, players);
    const std::array first_tick{PlayerInput{1, {1.0, 0.0}}};
    const std::array third_tick{
        PlayerInput{1, {0.0, 1.0}},
        PlayerInput{2, {-1.0, 0.0}},
    };

    EXPECT_EQ(session.record_tick(first_tick).error, ReplayError::none);
    EXPECT_EQ(session.record_tick({}).error, ReplayError::none);
    EXPECT_EQ(session.record_tick(third_tick).error, ReplayError::none);

    const auto events = session.recorded_ticks();
    ASSERT_EQ(events.size(), 3U);
    EXPECT_EQ(events[0].schema_version, 1U);
    EXPECT_EQ(events[0].tick, 1U);
    EXPECT_EQ(events[0].inputs.size(), 1U);
    EXPECT_EQ(events[1].tick, 2U);
    EXPECT_TRUE(events[1].inputs.empty());
    EXPECT_EQ(events[2].tick, 3U);

    const ReplayResult replay = session.validate_and_replay();
    ASSERT_EQ(replay.error, ReplayError::none);
    EXPECT_EQ(replay.final_tick, session.current_tick());
    ASSERT_EQ(replay.players.size(), players.size());
    for (const PlayerState& replayed_player : replay.players) {
        const auto current = session.current_player(replayed_player.id);
        ASSERT_TRUE(current);
        EXPECT_DOUBLE_EQ(replayed_player.position.x, current->position.x);
        EXPECT_DOUBLE_EQ(replayed_player.position.y, current->position.y);
        EXPECT_DOUBLE_EQ(replayed_player.velocity.x, current->velocity.x);
        EXPECT_DOUBLE_EQ(replayed_player.velocity.y, current->velocity.y);
    }
}

TEST(ReplaySessionTest, DoesNotRecordOrApplyRejectedInputs) {
    const std::array players{ReplayPlayer{1, {10.0, 10.0}}};
    ReplaySession session({}, players);
    const std::array invalid{PlayerInput{1, {2.0, 0.0}}};

    const ReplayRecordResult result = session.record_tick(invalid);

    EXPECT_EQ(result.error, ReplayError::simulation_rejected);
    EXPECT_EQ(result.simulation_error, SimulationError::invalid_input);
    EXPECT_EQ(session.current_tick(), 0U);
    EXPECT_TRUE(session.recorded_ticks().empty());
    const auto player = session.current_player(1);
    ASSERT_TRUE(player);
    EXPECT_DOUBLE_EQ(player->position.x, 10.0);
}

TEST(ReplaySessionTest, RejectsTicksAfterTheConfiguredCapacity) {
    const std::array players{ReplayPlayer{1, {10.0, 10.0}}};
    ReplaySessionConfig config;
    config.maximum_ticks = 1;
    ReplaySession session(config, players);

    EXPECT_EQ(session.record_tick({}).error, ReplayError::none);
    EXPECT_EQ(
        session.record_tick({}).error,
        ReplayError::tick_capacity_exceeded);
    EXPECT_EQ(session.current_tick(), 1U);
    EXPECT_EQ(session.recorded_ticks().size(), 1U);
}

TEST(ReplaySessionTest, RejectsTicksWithMoreInputsThanPlayers) {
    const std::array players{ReplayPlayer{1, {10.0, 10.0}}};
    ReplaySession session({}, players);
    const std::array inputs{
        PlayerInput{1, {1.0, 0.0}},
        PlayerInput{1, {0.0, 1.0}},
    };

    const ReplayRecordResult result = session.record_tick(inputs);

    EXPECT_EQ(result.error, ReplayError::input_limit_exceeded);
    EXPECT_EQ(session.current_tick(), 0U);
    EXPECT_TRUE(session.recorded_ticks().empty());
}

TEST(ReplaySessionTest, RejectsInvalidSessionLimitsAndInitialPlayers) {
    const std::array valid_player{ReplayPlayer{1, {10.0, 10.0}}};
    ReplaySessionConfig no_ticks;
    no_ticks.maximum_ticks = 0;
    EXPECT_THROW(ReplaySession session(no_ticks, valid_player), std::invalid_argument);

    const std::array duplicate_players{
        ReplayPlayer{1, {10.0, 10.0}},
        ReplayPlayer{1, {20.0, 20.0}},
    };
    EXPECT_THROW(
        ReplaySession session({}, duplicate_players),
        std::invalid_argument);

    ReplaySessionConfig oversized;
    oversized.maximum_ticks = kMaximumReplayInputRecords / 2 + 1;
    const std::array two_players{
        ReplayPlayer{1, {10.0, 10.0}},
        ReplayPlayer{2, {20.0, 20.0}},
    };
    EXPECT_THROW(ReplaySession session(oversized, two_players), std::invalid_argument);
}

}  // namespace
}  // namespace aegisguard
