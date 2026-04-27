#include "body_control/lighting/application/command_arbitrator.hpp"

namespace body_control::lighting::application
{

namespace
{

// Both kActivate and kToggle express the intent to turn a lamp on; kToggle
// is treated as an activation request for conflict detection because if the
// opposite indicator is already on, a toggle would activate this one anyway.
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

// Rejects commands before applying business rules so the rule branches never
// have to handle degenerate inputs.  kUnknown function and kNoAction are
// checked separately because both are structurally valid per IsValidLampCommand()
// but neither represents a meaningful operator intent.
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

// Constructs an expanded command from a parent, inheriting source and
// sequence_counter so the rear node can correlate all derived commands
// back to the same originating operator action.
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

    // ── Rule 1: Hazard expansion ──────────────────────────────────────────
    // A single hazard command fans out to three: hazard + left + right.
    // All three carry the same resolved action so they activate or deactivate
    // together atomically from the rear node's perspective.
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

    // ── Rule 2: Hazard blocks indicator activation ────────────────────────
    // While hazard is active all four indicators are already flashing; an
    // independent indicator command from the HMI would create a confusing
    // asymmetric flash pattern, so it is rejected outright.
    if (context.hazard_lamp_active && IsIndicatorActivationCommand(requested_command))
    {
        decision.result = ArbitrationResult::kRejected;
        return decision;
    }

    // ── Rule 3: Left indicator exclusivity ───────────────────────────────
    // Activating the left indicator while the right is on would produce an
    // illegal simultaneous bilateral flash.  Deactivate the right first,
    // then activate left.  The resolved action is always kActivate (not the
    // original kToggle) to prevent the toggle from being re-interpreted later.
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

    // ── Rule 4: Right indicator exclusivity (symmetric) ──────────────────
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

    // ── Default: no conflict, forward unchanged ───────────────────────────
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
    // kToggle needs to be resolved to a concrete activate or deactivate before
    // expansion so all three derived commands carry the same action.
    if (command.action == domain::LampCommandAction::kToggle)
    {
        return context.hazard_lamp_active
                   ? domain::LampCommandAction::kDeactivate
                   : domain::LampCommandAction::kActivate;
    }
    return command.action;
}

} // namespace body_control::lighting::application
