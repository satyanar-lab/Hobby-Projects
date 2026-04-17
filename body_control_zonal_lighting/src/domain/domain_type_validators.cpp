/**
 * @file domain_type_validators.cpp
 * @brief Structural validators for domain value types.
 *
 * Kept deliberately small and free-function to support MISRA-style use from
 * both the codec (encode/decode) and the managers (before caching).
 */

#include "body_control/lighting/domain/lamp_command_types.hpp"
#include "body_control/lighting/domain/lamp_status_types.hpp"

#include <cstdint>

namespace body_control::lighting::domain
{

namespace
{

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

    return IsInClosedRange(health_state_value, kNodeHealthStateMaxValue);
}

}  // namespace body_control::lighting::domain
