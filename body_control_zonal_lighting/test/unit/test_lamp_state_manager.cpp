/**
 * @file test_lamp_state_manager.cpp
 * @brief Unit tests for the LampStateManager cache and derived state.
 */

#include <gtest/gtest.h>

#include "body_control/lighting/application/lamp_state_manager.hpp"

namespace
{

using body_control::lighting::application::LampStateManager;
using body_control::lighting::domain::LampFunction;
using body_control::lighting::domain::LampOutputState;
using body_control::lighting::domain::LampStatus;

LampStatus MakeStatus(
    const LampFunction function,
    const LampOutputState output_state,
    const std::uint16_t sequence_counter)
{
    LampStatus status {};
    status.function = function;
    status.output_state = output_state;
    status.command_applied = true;
    status.last_sequence_counter = sequence_counter;
    return status;
}

}  // namespace

TEST(LampStateManagerTest, InitialCacheReportsEveryFunctionAsUnknown)
{
    const LampStateManager manager {};

    for (const LampFunction function : {
             LampFunction::kLeftIndicator,
             LampFunction::kRightIndicator,
             LampFunction::kHazardLamp,
             LampFunction::kParkLamp,
             LampFunction::kHeadLamp})
    {
        LampStatus status {};
        ASSERT_TRUE(manager.GetLampStatus(function, status));
        EXPECT_EQ(status.function, function);
        EXPECT_EQ(status.output_state, LampOutputState::kUnknown);
        EXPECT_FALSE(status.command_applied);
        EXPECT_EQ(status.last_sequence_counter, 0U);
    }
}

TEST(LampStateManagerTest, UpdateAndGetRoundTrip)
{
    LampStateManager manager {};

    const LampStatus incoming =
        MakeStatus(LampFunction::kHazardLamp, LampOutputState::kOn, 42U);

    ASSERT_TRUE(manager.UpdateLampStatus(incoming));

    LampStatus retrieved {};
    ASSERT_TRUE(manager.GetLampStatus(LampFunction::kHazardLamp, retrieved));

    EXPECT_EQ(retrieved.function, incoming.function);
    EXPECT_EQ(retrieved.output_state, incoming.output_state);
    EXPECT_TRUE(retrieved.command_applied);
    EXPECT_EQ(retrieved.last_sequence_counter, 42U);
}

TEST(LampStateManagerTest, UpdateRejectsUnknownFunction)
{
    LampStateManager manager {};

    const LampStatus unknown_status =
        MakeStatus(LampFunction::kUnknown, LampOutputState::kOn, 1U);

    EXPECT_FALSE(manager.UpdateLampStatus(unknown_status));
}

TEST(LampStateManagerTest, GetRejectsUnknownFunction)
{
    const LampStateManager manager {};

    LampStatus retrieved {};
    EXPECT_FALSE(
        manager.GetLampStatus(LampFunction::kUnknown, retrieved));
}

TEST(LampStateManagerTest, IsFunctionActiveFollowsReportedOutputState)
{
    LampStateManager manager {};

    const LampStatus on_status =
        MakeStatus(LampFunction::kHeadLamp, LampOutputState::kOn, 1U);
    ASSERT_TRUE(manager.UpdateLampStatus(on_status));
    EXPECT_TRUE(manager.IsFunctionActive(LampFunction::kHeadLamp));

    const LampStatus off_status =
        MakeStatus(LampFunction::kHeadLamp, LampOutputState::kOff, 2U);
    ASSERT_TRUE(manager.UpdateLampStatus(off_status));
    EXPECT_FALSE(manager.IsFunctionActive(LampFunction::kHeadLamp));
}

TEST(LampStateManagerTest, ArbitrationContextMirrorsHazardLampState)
{
    LampStateManager manager {};

    EXPECT_FALSE(manager.GetArbitrationContext().hazard_lamp_active);

    const LampStatus hazard_on =
        MakeStatus(LampFunction::kHazardLamp, LampOutputState::kOn, 10U);
    ASSERT_TRUE(manager.UpdateLampStatus(hazard_on));

    EXPECT_TRUE(manager.GetArbitrationContext().hazard_lamp_active);
}

TEST(LampStateManagerTest, ResetReturnsCacheToInitialShape)
{
    LampStateManager manager {};

    const LampStatus hazard_on =
        MakeStatus(LampFunction::kHazardLamp, LampOutputState::kOn, 99U);
    ASSERT_TRUE(manager.UpdateLampStatus(hazard_on));
    ASSERT_TRUE(manager.IsFunctionActive(LampFunction::kHazardLamp));

    manager.Reset();

    LampStatus after_reset {};
    ASSERT_TRUE(
        manager.GetLampStatus(LampFunction::kHazardLamp, after_reset));
    EXPECT_EQ(after_reset.output_state, LampOutputState::kUnknown);
    EXPECT_FALSE(after_reset.command_applied);
    EXPECT_EQ(after_reset.last_sequence_counter, 0U);
}
