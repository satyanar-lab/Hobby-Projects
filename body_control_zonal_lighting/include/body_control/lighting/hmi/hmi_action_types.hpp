#ifndef BODY_CONTROL_LIGHTING_HMI_HMI_ACTION_TYPES_HPP
#define BODY_CONTROL_LIGHTING_HMI_HMI_ACTION_TYPES_HPP

#include <cstdint>

namespace body_control
{
namespace lighting
{
namespace hmi
{

enum class HmiAction : std::uint8_t
{
    kUnknown = 0U,
    kToggleLeftIndicator = 1U,
    kToggleRightIndicator = 2U,
    kToggleHazardLamp = 3U,
    kToggleParkLamp = 4U,
    kToggleHeadLamp = 5U,
    kRequestNodeHealth = 6U
};

}  // namespace hmi
}  // namespace lighting
}  // namespace body_control

#endif  // BODY_CONTROL_LIGHTING_HMI_HMI_ACTION_TYPES_HPP