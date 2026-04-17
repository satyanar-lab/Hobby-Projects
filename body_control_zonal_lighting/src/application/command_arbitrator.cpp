#include "body_control/lighting/application/command_arbitrator.hpp"

namespace body_control::lighting::application
{

namespace
{

[[nodiscard]] bool IsActionKnownAndActive(
    const domain::LampCommandAction action) noexcept
{
    switch (action)
    {
    case domain::LampCommandAction::kActivate:
    case domain::LampCommandAction::kToggle:
        return true;

    case domain::LampCommandAction::kDeactivate:
    case domain::LampCommandAction::kNoAction:
    default:
        return false;
    }
}

[[nodiscard]] bool IsCommandStructurallyAccepted(
    const domain::LampCommand& command) noexcept
{
    // A structurally valid command still has to name a real function
    // and carry a non-kNoAction action to be arbitrated.
    if (!domain::IsValidLampCommand(command))
    {
        return false;
    }

    if (command.function == domain::LampFunction::kUnknown)
    {
        return false;
    }

    if (command.action == domain::LampCommandAction::kNoAction)
    {
        return false;
    }

    return true;
}

}  // namespace

ArbitrationDecision CommandArbitrator::Arbitrate(
    const domain::LampCommand& requested_command,
    const ArbitrationContext& context) const noexcept
{
    ArbitrationDecision decision {};
    decision.result = ArbitrationResult::kRejected;
    decision.command = requested_command;

    if (!IsCommandStructurallyAccepted(requested_command))
    {
        return decision;
    }

    if (IsHazardActivationCommand(requested_command))
    {
        decision.result = ArbitrationResult::kAccepted;
        return decision;
    }

    if (context.hazard_lamp_active &&
        IsIndicatorActivationCommand(requested_command))
    {
        decision.result = ArbitrationResult::kRejected;
        return decision;
    }

    decision.result = ArbitrationResult::kAccepted;
    return decision;
}

bool CommandArbitrator::IsHazardActivationCommand(
    const domain::LampCommand& command) const noexcept
{
    return (command.function == domain::LampFunction::kHazardLamp) &&
           IsActionKnownAndActive(command.action);
}

bool CommandArbitrator::IsIndicatorActivationCommand(
    const domain::LampCommand& command) const noexcept
{
    const bool is_indicator_function =
        (command.function == domain::LampFunction::kLeftIndicator) ||
        (command.function == domain::LampFunction::kRightIndicator);

    return is_indicator_function && IsActionKnownAndActive(command.action);
}

}  // namespace body_control::lighting::application
