/**
 * @file test_rear_lighting_function_manager.cpp
 * @brief Unit tests for the rear-node function manager behavior.
 *
 * Covered:
 *   1. Initial output states are Off for all managed functions.
 *   2. Activate / deactivate / toggle produce the expected output_state.
 *   3. kNoAction and kUnknown function are rejected.
 *   4. Sequence counter is stored on the status and returned by GetLampStatus.
 */

#include <gtest/gtest.h>

#include "body_control/lighting/application/rear_lighting_function_manager.hpp"

namespace
{

using body_control::lighting::application::RearLightingFunctionManager;
using body_control::lighting::domain::CommandSource;
using body_control::lighting::domain::LampCommand;
using body_control::lighting::domain::LampCommandAction;
using body_control::lighting::domain::LampFunction;
using body_control::lighting::domain::LampOutputState;
using body_control::lighting::domain::LampStatus;

LampCommand MakeCommand(
    const LampFunction function,
    const LampCommandAction action,
    const std::uint16_t sequence_counter)
{
    LampCommand command {};
    command.function = function;
    command.action = action;
    command.source = CommandSource::kCentralZoneController;
    command.sequence_counter = sequence_counter;
    return command;
}

}  // namespace

TEST(RearLightingFunctionManagerTest, InitialOutputsAreOff)
{
    const RearLightingFunctionManager manager {};

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
        EXPECT_EQ(status.output_state, LampOutputState::kOff);
    }
}

TEST(RearLightingFunctionManagerTest, ActivateTurnsLampOn)
{
    RearLightingFunctionManager manager {};

    const LampCommand command = MakeCommand(
        LampFunction::kHeadLamp, LampCommandAction::kActivate, 5U);
    ASSERT_TRUE(manager.ApplyCommand(command));

    LampStatus status {};
    ASSERT_TRUE(manager.GetLampStatus(LampFunction::kHeadLamp, status));
    EXPECT_EQ(status.output_state, LampOutputState::kOn);
    EXPECT_TRUE(status.command_applied);
    EXPECT_EQ(status.last_sequence_counter, 5U);
}

TEST(RearLightingFunctionManagerTest, DeactivateTurnsLampOff)
{
    RearLightingFunctionManager manager {};

    ASSERT_TRUE(manager.ApplyCommand(MakeCommand(
        LampFunction::kParkLamp, LampCommandAction::kActivate, 1U)));
    ASSERT_TRUE(manager.ApplyCommand(MakeCommand(
        LampFunction::kParkLamp, LampCommandAction::kDeactivate, 2U)));

    LampStatus status {};
    ASSERT_TRUE(manager.GetLampStatus(LampFunction::kParkLamp, status));
    EXPECT_EQ(status.output_state, LampOutputState::kOff);
    EXPECT_EQ(status.last_sequence_counter, 2U);
}

TEST(RearLightingFunctionManagerTest, ToggleInvertsOutputState)
{
    RearLightingFunctionManager manager {};

    LampStatus status {};
    ASSERT_TRUE(manager.GetLampStatus(LampFunction::kLeftIndicator, status));
    ASSERT_EQ(status.output_state, LampOutputState::kOff);

    ASSERT_TRUE(manager.ApplyCommand(MakeCommand(
        LampFunction::kLeftIndicator, LampCommandAction::kToggle, 1U)));
    ASSERT_TRUE(manager.GetLampStatus(LampFunction::kLeftIndicator, status));
    EXPECT_EQ(status.output_state, LampOutputState::kOn);

    ASSERT_TRUE(manager.ApplyCommand(MakeCommand(
        LampFunction::kLeftIndicator, LampCommandAction::kToggle, 2U)));
    ASSERT_TRUE(manager.GetLampStatus(LampFunction::kLeftIndicator, status));
    EXPECT_EQ(status.output_state, LampOutputState::kOff);
}

TEST(RearLightingFunctionManagerTest, NoActionCommandIsRejected)
{
    RearLightingFunctionManager manager {};

    const LampCommand command = MakeCommand(
        LampFunction::kHeadLamp, LampCommandAction::kNoAction, 1U);

    EXPECT_FALSE(manager.ApplyCommand(command));
}

TEST(RearLightingFunctionManagerTest, UnknownFunctionIsRejected)
{
    RearLightingFunctionManager manager {};

    const LampCommand command = MakeCommand(
        LampFunction::kUnknown, LampCommandAction::kActivate, 1U);

    EXPECT_FALSE(manager.ApplyCommand(command));
}

TEST(RearLightingFunctionManagerTest, GetLampStatusRejectsUnknownFunction)
{
    const RearLightingFunctionManager manager {};

    LampStatus status {};
    EXPECT_FALSE(
        manager.GetLampStatus(LampFunction::kUnknown, status));
}
