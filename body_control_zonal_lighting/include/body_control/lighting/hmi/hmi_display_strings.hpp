#ifndef BODY_CONTROL_LIGHTING_HMI_HMI_DISPLAY_STRINGS_HPP
#define BODY_CONTROL_LIGHTING_HMI_HMI_DISPLAY_STRINGS_HPP

#include "body_control/lighting/domain/lamp_command_types.hpp"
#include "body_control/lighting/domain/lamp_status_types.hpp"

namespace body_control::lighting::hmi
{

/** Returns a human-readable label for a lamp function (e.g. "LeftIndicator").
 *  Returns "Unknown" for kUnknown or any unmapped value. */
[[nodiscard]] const char* LampFunctionToString(
    domain::LampFunction function) noexcept;

/** Returns a human-readable label for a lamp output state (e.g. "On", "Off").
 *  Returns "Unknown" for kUnknown or any unmapped value. */
[[nodiscard]] const char* LampOutputStateToString(
    domain::LampOutputState output_state) noexcept;

/** Returns a human-readable label for a node health state (e.g. "Operational").
 *  Returns "Unknown" for kUnknown or any unmapped value. */
[[nodiscard]] const char* NodeHealthStateToString(
    domain::NodeHealthState health_state) noexcept;

}  // namespace body_control::lighting::hmi

#endif  // BODY_CONTROL_LIGHTING_HMI_HMI_DISPLAY_STRINGS_HPP
