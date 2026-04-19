/**
 * @file test_command_arbitrator.cpp
 * @brief Unit tests for the CommandArbitrator arbitration rules.
 *
 * Covered behaviors:
 *   1. Structurally invalid commands are rejected.
 *   2. A kNoAction command is rejected even if every other field is valid.
 *   3. Hazard activation expands to three commands (hazard + left + right).
 *   4. Hazard deactivation expands to three commands (hazard + left + right).
 *   5. Indicator activation is rejected while hazard is active.
 *   6. Indicator deactivation is accepted even while hazard is active.
 *   7. Park and head lamp commands are independent of hazard state.
 *   8. Accepted single-command decision preserves original command fields.
 *   9. Left indicator activation auto-deactivates right when right is active.
 *  10. Right indicator activation auto-deactivates left when left is active.
 */

#include <gtest/gtest.h>

#include "body_control/lighting/application/command_arbitrator.hpp"

namespace
{

using body_control::lighting::application::ArbitrationContext;
using body_control::lighting::application::ArbitrationResult;
using body_control::lighting::application::CommandArbitrator;
using body_control::lighting::domain::CommandSource;
using body_control::lighting::domain::LampCommand;
using body_control::lighting::domain::LampCommandAction;
using body_control::lighting::domain::LampFunction;

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

} // namespace

TEST(CommandArbitratorTest, RejectsCommandWithUnknownFunction)
{
    const CommandArbitrator arbitrator {};
    const ArbitrationContext context {};

    const LampCommand command =
        MakeCommand(LampFunction::kUnknown, LampCommandAction::kActivate);

    const auto decision = arbitrator.Arbitrate(command, context);

    EXPECT_EQ(decision.result, ArbitrationResult::kRejected);
    EXPECT_EQ(decision.command_count, 0U);
}

TEST(CommandArbitratorTest, RejectsNoActionCommand)
{
    const CommandArbitrator arbitrator {};
    const ArbitrationContext context {};

    const LampCommand command =
        MakeCommand(LampFunction::kLeftIndicator, LampCommandAction::kNoAction);

    const auto decision = arbitrator.Arbitrate(command, context);

    EXPECT_EQ(decision.result, ArbitrationResult::kRejected);
    EXPECT_EQ(decision.command_count, 0U);
}

TEST(CommandArbitratorTest, HazardActivationProducesThreeCommands)
{
    const CommandArbitrator arbitrator {};
    ArbitrationContext context {};
    context.left_indicator_active = true;
    context.right_indicator_active = true;

    const LampCommand command =
        MakeCommand(LampFunction::kHazardLamp, LampCommandAction::kActivate);

    const auto decision = arbitrator.Arbitrate(command, context);

    EXPECT_EQ(decision.result, ArbitrationResult::kAccepted);
    ASSERT_EQ(decision.command_count, 3U);
    EXPECT_EQ(decision.commands[0U].function, LampFunction::kHazardLamp);
    EXPECT_EQ(decision.commands[0U].action,   LampCommandAction::kActivate);
    EXPECT_EQ(decision.commands[1U].function, LampFunction::kLeftIndicator);
    EXPECT_EQ(decision.commands[1U].action,   LampCommandAction::kActivate);
    EXPECT_EQ(decision.commands[2U].function, LampFunction::kRightIndicator);
    EXPECT_EQ(decision.commands[2U].action,   LampCommandAction::kActivate);
}

TEST(CommandArbitratorTest, HazardDeactivationProducesThreeCommands)
{
    const CommandArbitrator arbitrator {};
    ArbitrationContext context {};
    context.hazard_lamp_active = true;

    const LampCommand command =
        MakeCommand(LampFunction::kHazardLamp, LampCommandAction::kDeactivate);

    const auto decision = arbitrator.Arbitrate(command, context);

    EXPECT_EQ(decision.result, ArbitrationResult::kAccepted);
    ASSERT_EQ(decision.command_count, 3U);
    EXPECT_EQ(decision.commands[0U].function, LampFunction::kHazardLamp);
    EXPECT_EQ(decision.commands[0U].action,   LampCommandAction::kDeactivate);
    EXPECT_EQ(decision.commands[1U].function, LampFunction::kLeftIndicator);
    EXPECT_EQ(decision.commands[1U].action,   LampCommandAction::kDeactivate);
    EXPECT_EQ(decision.commands[2U].function, LampFunction::kRightIndicator);
    EXPECT_EQ(decision.commands[2U].action,   LampCommandAction::kDeactivate);
}

TEST(CommandArbitratorTest, RejectsIndicatorActivationWhenHazardActive)
{
    const CommandArbitrator arbitrator {};
    ArbitrationContext context {};
    context.hazard_lamp_active = true;

    const LampCommand left_on =
        MakeCommand(LampFunction::kLeftIndicator, LampCommandAction::kActivate);
    const LampCommand right_on =
        MakeCommand(LampFunction::kRightIndicator, LampCommandAction::kActivate);

    EXPECT_EQ(arbitrator.Arbitrate(left_on, context).result,
              ArbitrationResult::kRejected);
    EXPECT_EQ(arbitrator.Arbitrate(right_on, context).result,
              ArbitrationResult::kRejected);
}

TEST(CommandArbitratorTest, AcceptsIndicatorDeactivationEvenWhenHazardActive)
{
    const CommandArbitrator arbitrator {};
    ArbitrationContext context {};
    context.hazard_lamp_active = true;

    const LampCommand left_off =
        MakeCommand(LampFunction::kLeftIndicator, LampCommandAction::kDeactivate);

    const auto decision = arbitrator.Arbitrate(left_off, context);

    EXPECT_EQ(decision.result, ArbitrationResult::kAccepted);
    EXPECT_EQ(decision.command_count, 1U);
}

TEST(CommandArbitratorTest, ParkAndHeadLampAreIndependentOfHazardState)
{
    const CommandArbitrator arbitrator {};
    ArbitrationContext context {};
    context.hazard_lamp_active = true;

    const LampCommand park_on =
        MakeCommand(LampFunction::kParkLamp, LampCommandAction::kActivate);
    const LampCommand head_on =
        MakeCommand(LampFunction::kHeadLamp, LampCommandAction::kActivate);

    EXPECT_EQ(arbitrator.Arbitrate(park_on, context).result,
              ArbitrationResult::kAccepted);
    EXPECT_EQ(arbitrator.Arbitrate(head_on, context).result,
              ArbitrationResult::kAccepted);
}

TEST(CommandArbitratorTest, AcceptedDecisionPreservesOriginalCommand)
{
    const CommandArbitrator arbitrator {};
    const ArbitrationContext context {};

    const LampCommand command =
        MakeCommand(LampFunction::kHeadLamp, LampCommandAction::kActivate);

    const auto decision = arbitrator.Arbitrate(command, context);

    EXPECT_EQ(decision.result, ArbitrationResult::kAccepted);
    ASSERT_EQ(decision.command_count, 1U);
    EXPECT_EQ(decision.commands[0U].function,         command.function);
    EXPECT_EQ(decision.commands[0U].action,           command.action);
    EXPECT_EQ(decision.commands[0U].source,           command.source);
    EXPECT_EQ(decision.commands[0U].sequence_counter, command.sequence_counter);
}

TEST(CommandArbitratorTest, LeftIndicatorDeactivatesRightWhenRightIsActive)
{
    const CommandArbitrator arbitrator {};
    ArbitrationContext context {};
    context.right_indicator_active = true;

    const LampCommand left_on =
        MakeCommand(LampFunction::kLeftIndicator, LampCommandAction::kActivate);

    const auto decision = arbitrator.Arbitrate(left_on, context);

    EXPECT_EQ(decision.result, ArbitrationResult::kModified);
    ASSERT_EQ(decision.command_count, 2U);
    EXPECT_EQ(decision.commands[0U].function, LampFunction::kRightIndicator);
    EXPECT_EQ(decision.commands[0U].action,   LampCommandAction::kDeactivate);
    EXPECT_EQ(decision.commands[1U].function, LampFunction::kLeftIndicator);
    EXPECT_EQ(decision.commands[1U].action,   LampCommandAction::kActivate);
}

TEST(CommandArbitratorTest, RightIndicatorDeactivatesLeftWhenLeftIsActive)
{
    const CommandArbitrator arbitrator {};
    ArbitrationContext context {};
    context.left_indicator_active = true;

    const LampCommand right_on =
        MakeCommand(LampFunction::kRightIndicator, LampCommandAction::kActivate);

    const auto decision = arbitrator.Arbitrate(right_on, context);

    EXPECT_EQ(decision.result, ArbitrationResult::kModified);
    ASSERT_EQ(decision.command_count, 2U);
    EXPECT_EQ(decision.commands[0U].function, LampFunction::kLeftIndicator);
    EXPECT_EQ(decision.commands[0U].action,   LampCommandAction::kDeactivate);
    EXPECT_EQ(decision.commands[1U].function, LampFunction::kRightIndicator);
    EXPECT_EQ(decision.commands[1U].action,   LampCommandAction::kActivate);
}
