#ifndef BODY_CONTROL_LIGHTING_DOMAIN_LAMP_COMMAND_TYPES_HPP_
#define BODY_CONTROL_LIGHTING_DOMAIN_LAMP_COMMAND_TYPES_HPP_

#include <cstdint>

namespace body_control::lighting::domain
{

/**
 * @brief Supported lighting functions handled by the Rear Lighting Node.
 */
enum class LampFunction : std::uint8_t
{
    kLeftIndicator  = 0U,
    kRightIndicator = 1U,
    kHazardLamp     = 2U,
    kParkLamp       = 3U,
    kHeadLamp       = 4U,
    kUnknown        = 255U
};

/**
 * @brief Requested action for a lighting function.
 */
enum class LampCommandAction : std::uint8_t
{
    kDeactivate = 0U,
    kActivate   = 1U,
    kUnknown    = 255U
};

/**
 * @brief Origin of the command.
 *
 * This is useful for traceability and later arbitration policy.
 */
enum class CommandSource : std::uint8_t
{
    kHmiControlPanel      = 0U,
    kCentralZoneLogic     = 1U,
    kDiagnosticConsole    = 2U,
    kAutomatedTest        = 3U,
    kUnknown              = 255U
};

/**
 * @brief Logical lamp command sent from the controller side.
 */
struct LampCommand
{
    LampFunction function {LampFunction::kUnknown};
    LampCommandAction action {LampCommandAction::kUnknown};
    CommandSource source {CommandSource::kUnknown};
    std::uint16_t sequence_counter {0U};
};

constexpr bool IsValidLampFunction(const LampFunction function) noexcept
{
    return (function == LampFunction::kLeftIndicator)  ||
           (function == LampFunction::kRightIndicator) ||
           (function == LampFunction::kHazardLamp)     ||
           (function == LampFunction::kParkLamp)       ||
           (function == LampFunction::kHeadLamp);
}

constexpr bool IsValidLampCommandAction(const LampCommandAction action) noexcept
{
    return (action == LampCommandAction::kDeactivate) ||
           (action == LampCommandAction::kActivate);
}

constexpr bool IsValidCommandSource(const CommandSource source) noexcept
{
    return (source == CommandSource::kHmiControlPanel)   ||
           (source == CommandSource::kCentralZoneLogic)  ||
           (source == CommandSource::kDiagnosticConsole) ||
           (source == CommandSource::kAutomatedTest);
}

constexpr bool IsIndicatorFunction(const LampFunction function) noexcept
{
    return (function == LampFunction::kLeftIndicator) ||
           (function == LampFunction::kRightIndicator);
}

constexpr bool IsBlinkingFunction(const LampFunction function) noexcept
{
    return IsIndicatorFunction(function) ||
           (function == LampFunction::kHazardLamp);
}

constexpr bool IsValidLampCommand(const LampCommand& command) noexcept
{
    return IsValidLampFunction(command.function) &&
           IsValidLampCommandAction(command.action) &&
           IsValidCommandSource(command.source);
}

}  // namespace body_control::lighting::domain

#endif  // BODY_CONTROL_LIGHTING_DOMAIN_LAMP_COMMAND_TYPES_HPP_