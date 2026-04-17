/**
 * @file test_hazard_priority_behavior.cpp
 * @brief Integration-level check that hazard priority arbitration holds
 *        when the arbitrator is driven from real LampStateManager context.
 *
 * Controller-side arbitration is what guarantees that activating hazard
 * locks out indicator activation commands until hazard is deactivated.
 * This integration test wires the real CommandArbitrator against an
 * ArbitrationContext produced by LampStateManager so the two-component
 * contract is exercised, not just the arbitrator in isolation.
 */

#include <gtest/gtest.h>

#include "body_control/lighting/application/command_arbitrator.hpp"
#include "body_control/lighting/application/lamp_state_manager.hpp"

namespace
{

using body_control::lighting::application::ArbitrationResult;
using body_control::lighting::application::CommandArbitrator;
using body_control::lighting::application::LampStateManager;
using body_control::lighting::domain::CommandSource;
using body_control::lighting::domain::LampCommand;
using body_control::lighting::domain::LampCommandAction;
using body_control::lighting::domain::LampFunction;
using body_control::lighting::domain::LampOutputState;
using body_control::lighting::domain::LampStatus;

LampCommand MakeCommand(
    const LampFunction function,
    const LampCommandAction action)
{
    LampCommand command {};
    command.function = function;
    command.action = action;
    command.source = CommandSource::kHmiControlPanel;
    command.sequence_counter = 1U;
    return command;
}

LampStatus MakeActiveStatus(const LampFunction function)
{
    LampStatus status {};
    status.function = function;
    status.output_state = LampOutputState::kOn;
    status.command_applied = true;
    status.last_sequence_counter = 1U;
    return status;
}

}  // namespace

TEST(HazardPriorityIntegration, IndicatorLockedOutWhenHazardStateReportsActive)
{
    LampStateManager state_manager {};
    const CommandArbitrator arbitrator {};

    // Simulate the node having reported hazard as ON.
    ASSERT_TRUE(state_manager.UpdateLampStatus(
        MakeActiveStatus(LampFunction::kHazardLamp)));

    const auto context = state_manager.GetArbitrationContext();
    ASSERT_TRUE(context.hazard_lamp_active);

    const auto left_activate = MakeCommand(
        LampFunction::kLeftIndicator, LampCommandAction::kActivate);

    const auto decision = arbitrator.Arbitrate(left_activate, context);

    EXPECT_EQ(decision.result, ArbitrationResult::kRejected);
}

TEST(HazardPriorityIntegration, IndicatorAcceptedOnceHazardClears)
{
    LampStateManager state_manager {};
    const CommandArbitrator arbitrator {};

    // Hazard turned off: reported status has kOff output.
    LampStatus hazard_off {};
    hazard_off.function = LampFunction::kHazardLamp;
    hazard_off.output_state = LampOutputState::kOff;
    hazard_off.command_applied = true;
    hazard_off.last_sequence_counter = 2U;
    ASSERT_TRUE(state_manager.UpdateLampStatus(hazard_off));

    const auto context = state_manager.GetArbitrationContext();
    ASSERT_FALSE(context.hazard_lamp_active);

    const auto right_activate = MakeCommand(
        LampFunction::kRightIndicator, LampCommandAction::kActivate);

    const auto decision = arbitrator.Arbitrate(right_activate, context);

    EXPECT_EQ(decision.result, ArbitrationResult::kAccepted);
}
