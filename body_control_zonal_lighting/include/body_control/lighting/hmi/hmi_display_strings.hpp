#ifndef BODY_CONTROL_LIGHTING_HMI_HMI_DISPLAY_STRINGS_HPP
#define BODY_CONTROL_LIGHTING_HMI_HMI_DISPLAY_STRINGS_HPP

#include "body_control/lighting/domain/lamp_command_types.hpp"
#include "body_control/lighting/domain/lamp_status_types.hpp"

namespace body_control::lighting::hmi
{

[[nodiscard]] const char* LampFunctionToString(
    domain::LampFunction function) noexcept;

[[nodiscard]] const char* LampOutputStateToString(
    domain::LampOutputState output_state) noexcept;

[[nodiscard]] const char* NodeHealthStateToString(
    domain::NodeHealthState health_state) noexcept;

}  // namespace body_control::lighting::hmi

#endif  // BODY_CONTROL_LIGHTING_HMI_HMI_DISPLAY_STRINGS_HPP
