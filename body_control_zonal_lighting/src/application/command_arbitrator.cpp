#include "body_control/lighting/application/command_arbitrator.hpp"

namespace body_control::lighting::application
{

ArbitrationDecision CommandArbitrator::Arbitrate(
    const domain::LampCommand& requested_command,
    const ArbitrationContext& context) const noexcept
{
    ArbitrationDecision decision {};
    decision.command = requested_command;

    if (!domain::IsValidLampCommand(requested_command))
    {
        decision.result = ArbitrationResult::kRejected;
        return decision;
    }

    if (IsHazardActivationCommand(requested_command))
    {
        decision.result = ArbitrationResult::kAccepted;
        return decision;
    }

    if (context.hazard_lamp_active && IsIndicatorActivationCommand(requested_command))
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
           (command.action == domain::LampCommandAction::kActivate);
}

bool CommandArbitrator::IsIndicatorActivationCommand(
    const domain::LampCommand& command) const noexcept
{
    return domain::IsIndicatorFunction(command.function) &&
           (command.action == domain::LampCommandAction::kActivate);
}

}  // namespace body_control::lighting::application