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

[[nodiscard]] domain::LampCommand MakeDerived(
    const domain::LampCommand& source,
    const domain::LampFunction function,
    const domain::LampCommandAction action) noexcept
{
    domain::LampCommand derived {};
    derived.function         = function;
    derived.action           = action;
    derived.source           = source.source;
    derived.sequence_counter = source.sequence_counter;
    return derived;
}

} // namespace

// ---------------------------------------------------------------------------

ArbitrationDecision CommandArbitrator::Arbitrate(
    const domain::LampCommand& requested_command,
    const ArbitrationContext& context) const noexcept
{
    ArbitrationDecision decision {};
    decision.result        = ArbitrationResult::kRejected;
    decision.command_count = 0U;

    if (!IsCommandStructurallyAccepted(requested_command))
    {
        return decision;
    }

    // ---- Hazard ----
    if (IsHazardCommand(requested_command))
    {
        const domain::LampCommandAction resolved =
            ResolveHazardAction(requested_command, context);

        decision.result        = ArbitrationResult::kAccepted;
        decision.commands[0U]  = MakeDerived(
            requested_command, domain::LampFunction::kHazardLamp, resolved);
        decision.commands[1U]  = MakeDerived(
            requested_command, domain::LampFunction::kLeftIndicator, resolved);
        decision.commands[2U]  = MakeDerived(
            requested_command, domain::LampFunction::kRightIndicator, resolved);
        decision.command_count = 3U;
        return decision;
    }

    // ---- Indicator locked out by hazard ----
    if (context.hazard_lamp_active && IsIndicatorActivationCommand(requested_command))
    {
        decision.result = ArbitrationResult::kRejected;
        return decision;
    }

    // ---- Left indicator exclusivity ----
    if ((requested_command.function == domain::LampFunction::kLeftIndicator) &&
        IsActionKnownAndActive(requested_command.action) &&
        context.right_indicator_active)
    {
        decision.result        = ArbitrationResult::kModified;
        decision.commands[0U]  = MakeDerived(
            requested_command,
            domain::LampFunction::kRightIndicator,
            domain::LampCommandAction::kDeactivate);
        decision.commands[1U]  = requested_command;
        decision.commands[1U].action = domain::LampCommandAction::kActivate;
        decision.command_count = 2U;
        return decision;
    }

    // ---- Right indicator exclusivity ----
    if ((requested_command.function == domain::LampFunction::kRightIndicator) &&
        IsActionKnownAndActive(requested_command.action) &&
        context.left_indicator_active)
    {
        decision.result        = ArbitrationResult::kModified;
        decision.commands[0U]  = MakeDerived(
            requested_command,
            domain::LampFunction::kLeftIndicator,
            domain::LampCommandAction::kDeactivate);
        decision.commands[1U]  = requested_command;
        decision.commands[1U].action = domain::LampCommandAction::kActivate;
        decision.command_count = 2U;
        return decision;
    }

    // ---- Default: accept as-is ----
    decision.result        = ArbitrationResult::kAccepted;
    decision.commands[0U]  = requested_command;
    decision.command_count = 1U;
    return decision;
}

bool CommandArbitrator::IsHazardCommand(
    const domain::LampCommand& command) const noexcept
{
    return (command.function == domain::LampFunction::kHazardLamp);
}

bool CommandArbitrator::IsIndicatorActivationCommand(
    const domain::LampCommand& command) const noexcept
{
    const bool is_indicator =
        (command.function == domain::LampFunction::kLeftIndicator) ||
        (command.function == domain::LampFunction::kRightIndicator);

    return is_indicator && IsActionKnownAndActive(command.action);
}

domain::LampCommandAction CommandArbitrator::ResolveHazardAction(
    const domain::LampCommand& command,
    const ArbitrationContext& context) const noexcept
{
    if (command.action == domain::LampCommandAction::kToggle)
    {
        return context.hazard_lamp_active
                   ? domain::LampCommandAction::kDeactivate
                   : domain::LampCommandAction::kActivate;
    }
    return command.action;
}

} // namespace body_control::lighting::application
