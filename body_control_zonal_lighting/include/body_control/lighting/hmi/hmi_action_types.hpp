#ifndef BODY_CONTROL_LIGHTING_HMI_HMI_ACTION_TYPES_HPP
#define BODY_CONTROL_LIGHTING_HMI_HMI_ACTION_TYPES_HPP

#include <cstdint>

namespace body_control
{
namespace lighting
{
namespace hmi
{

/** Operator intent decoded from a raw keyboard or button input.
 *  Each value corresponds to a single user gesture; the command mapper
 *  translates raw input characters into these actions before passing them
 *  to the main window for dispatch to the operator service. */
enum class HmiAction : std::uint8_t
{
    kUnknown = 0U,              ///< Zero-sentinel; rejected without dispatch.
    kToggleLeftIndicator = 1U,  ///< Toggle left turn indicator on/off.
    kToggleRightIndicator = 2U, ///< Toggle right turn indicator on/off.
    kToggleHazardLamp = 3U,     ///< Toggle hazard (both indicators) on/off.
    kToggleParkLamp = 4U,       ///< Toggle parking lamp on/off.
    kToggleHeadLamp = 5U,       ///< Toggle head lamp on/off.
    kRequestNodeHealth = 6U     ///< Request a fresh node health snapshot.
};

}  // namespace hmi
}  // namespace lighting
}  // namespace body_control

#endif  // BODY_CONTROL_LIGHTING_HMI_HMI_ACTION_TYPES_HPP