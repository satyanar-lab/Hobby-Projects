#include "body_control/lighting/application/command_arbitrator.hpp"

namespace body_control
{
namespace lighting
{
namespace application
{

namespace
{

bool IsKnownLampFunction(
    const domain::LampFunction lamp_function) noexcept
{
    switch (lamp_function)
    {
    case domain::LampFunction::kLeftIndicator:
    case domain::LampFunction::kRightIndicator:
    case domain::LampFunction::kHazardLamp:
    case domain::LampFunction::kParkLamp:
    case domain::LampFunction::kHeadLamp:
        return true;

    case domain::LampFunction::kUnknown:
    default:
        return false;
    }
}

bool IsKnownLampCommandAction(
    const domain::LampCommandAction action) noexcept
{
    switch (action)
    {
    case domain::LampCommandAction::kActivate:
    case domain::LampCommandAction::kDeactivate:
    case domain::LampCommandAction::kToggle:
        return true;

    case domain::LampCommandAction::kNoAction:
    default:
        return false;
    }
}

bool IsKnownCommandSource(
    const domain::CommandSource source) noexcept
{
    switch (source)
    {
    case domain::CommandSource::kHmiControlPanel:
    case domain::CommandSource::kDiagnosticConsole:
    case domain::CommandSource::kCentralZoneController:
        return true;

    case domain::CommandSource::kUnknown:
    default:
        return false;
    }
}

bool IsValidLampCommand(
    const domain::LampCommand& command) noexcept
{
    return IsKnownLampFunction(command.function) &&
           IsKnownLampCommandAction(command.action) &&
           IsKnownCommandSource(command.source);
}

}  // namespace

ArbitrationDecision CommandArbitrator::Arbitrate(
    const domain::LampCommand& requested_command,
    const ArbitrationContext& context) const noexcept
{
    ArbitrationDecision decision {};
    decision.result = ArbitrationResult::kRejected;
    decision.command = requested_command;

    if (!IsValidLampCommand(requested_command))
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
           ((command.action == domain::LampCommandAction::kActivate) ||
            (command.action == domain::LampCommandAction::kToggle));
}

bool CommandArbitrator::IsIndicatorActivationCommand(
    const domain::LampCommand& command) const noexcept
{
    const bool is_indicator_function =
        (command.function == domain::LampFunction::kLeftIndicator) ||
        (command.function == domain::LampFunction::kRightIndicator);

    const bool is_activation_request =
        (command.action == domain::LampCommandAction::kActivate) ||
        (command.action == domain::LampCommandAction::kToggle);

    return is_indicator_function && is_activation_request;
}

}  // namespace application
}  // namespace lighting
}  // namespace body_control