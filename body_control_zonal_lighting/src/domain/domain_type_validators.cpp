/**
 * Structural validators for the core domain value types.
 *
 * Each validator casts every enum field to its underlying uint8_t and checks
 * it lies within the defined range.  Non-enum fields (sequence counters, fault
 * counts) are intentionally not validated here — any uint16_t value is valid
 * for those fields and range checking belongs with the business logic that
 * interprets them.
 *
 * These functions are called at two points:
 *   1. In the codec (before encoding, after decoding) to catch wire corruption.
 *   2. In the managers (before caching) to reject structurally invalid inputs.
 */

#include "body_control/lighting/domain/fault_types.hpp"
#include "body_control/lighting/domain/lamp_command_types.hpp"
#include "body_control/lighting/domain/lamp_status_types.hpp"

#include <cstdint>

namespace body_control::lighting::domain
{

namespace
{

// Upper-bound constants derived from the last enumerator of each enum.
// Computed via static_cast so they automatically stay correct if new values
// are appended to the enums in the future.
constexpr std::uint8_t kLampFunctionMaxValue {
    static_cast<std::uint8_t>(LampFunction::kHeadLamp)};

constexpr std::uint8_t kLampCommandActionMaxValue {
    static_cast<std::uint8_t>(LampCommandAction::kToggle)};

constexpr std::uint8_t kCommandSourceMaxValue {
    static_cast<std::uint8_t>(CommandSource::kCentralZoneController)};

constexpr std::uint8_t kLampOutputStateMaxValue {
    static_cast<std::uint8_t>(LampOutputState::kOn)};

constexpr std::uint8_t kNodeHealthStateMaxValue {
    static_cast<std::uint8_t>(NodeHealthState::kUnavailable)};

// All enums start at 0 so a single upper-bound check covers the full range.
[[nodiscard]] bool IsInClosedRange(
    const std::uint8_t value,
    const std::uint8_t inclusive_max) noexcept
{
    return value <= inclusive_max;
}

}  // namespace

bool IsValidLampCommand(const LampCommand& lamp_command) noexcept
{
    const std::uint8_t function_value =
        static_cast<std::uint8_t>(lamp_command.function);
    const std::uint8_t action_value =
        static_cast<std::uint8_t>(lamp_command.action);
    const std::uint8_t source_value =
        static_cast<std::uint8_t>(lamp_command.source);

    return IsInClosedRange(function_value, kLampFunctionMaxValue) &&
           IsInClosedRange(action_value, kLampCommandActionMaxValue) &&
           IsInClosedRange(source_value, kCommandSourceMaxValue);
}

bool IsValidLampStatus(const LampStatus& lamp_status) noexcept
{
    const std::uint8_t function_value =
        static_cast<std::uint8_t>(lamp_status.function);
    const std::uint8_t output_state_value =
        static_cast<std::uint8_t>(lamp_status.output_state);

    return IsInClosedRange(function_value, kLampFunctionMaxValue) &&
           IsInClosedRange(output_state_value, kLampOutputStateMaxValue);
}

bool IsValidNodeHealthStatus(
    const NodeHealthStatus& node_health_status) noexcept
{
    const std::uint8_t health_state_value =
        static_cast<std::uint8_t>(node_health_status.health_state);

    // Only the NodeHealthState enum field is range-checked; the boolean flags
    // and fault count are inherently valid for all bit patterns they can hold.
    return IsInClosedRange(health_state_value, kNodeHealthStateMaxValue);
}

FaultCode LampFunctionToFaultCode(const LampFunction function) noexcept
{
    switch (function)
    {
    case LampFunction::kLeftIndicator:  return FaultCode::kLeftIndicator;
    case LampFunction::kRightIndicator: return FaultCode::kRightIndicator;
    case LampFunction::kHazardLamp:     return FaultCode::kHazardLamp;
    case LampFunction::kParkLamp:       return FaultCode::kParkLamp;
    case LampFunction::kHeadLamp:       return FaultCode::kHeadLamp;
    default:                            return FaultCode::kNoFault;
    }
}

std::string_view FaultCodeToString(const FaultCode code) noexcept
{
    switch (code)
    {
    case FaultCode::kNoFault:        return "kNoFault";
    case FaultCode::kLeftIndicator:  return "B001:LeftIndicator";
    case FaultCode::kRightIndicator: return "B002:RightIndicator";
    case FaultCode::kHazardLamp:     return "B003:HazardLamp";
    case FaultCode::kParkLamp:       return "B004:ParkLamp";
    case FaultCode::kHeadLamp:       return "B005:HeadLamp";
    default:                         return "Unknown";
    }
}

bool IsValidFaultCommand(const FaultCommand& fault_command) noexcept
{
    constexpr std::uint8_t kFaultActionMaxValue {
        static_cast<std::uint8_t>(FaultAction::kClear)};

    const std::uint8_t function_value =
        static_cast<std::uint8_t>(fault_command.function);
    const std::uint8_t action_value =
        static_cast<std::uint8_t>(fault_command.action);
    const std::uint8_t source_value =
        static_cast<std::uint8_t>(fault_command.source);

    return IsInClosedRange(function_value, kLampFunctionMaxValue) &&
           IsInClosedRange(action_value, kFaultActionMaxValue) &&
           IsInClosedRange(source_value, kCommandSourceMaxValue);
}

}  // namespace body_control::lighting::domain
