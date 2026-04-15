#include "body_control/lighting/hmi/hmi_command_mapper.hpp"

namespace body_control
{
namespace lighting
{
namespace hmi
{

HmiAction HmiCommandMapper::MapInputToAction(
    const char input_key) noexcept
{
    HmiAction action {HmiAction::kUnknown};

    switch (input_key)
    {
    case '1':
        action = HmiAction::kToggleLeftIndicator;
        break;

    case '2':
        action = HmiAction::kToggleRightIndicator;
        break;

    case '3':
        action = HmiAction::kToggleHazardLamp;
        break;

    case '4':
        action = HmiAction::kToggleParkLamp;
        break;

    case '5':
        action = HmiAction::kToggleHeadLamp;
        break;

    case '6':
        action = HmiAction::kRequestNodeHealth;
        break;

    default:
        action = HmiAction::kUnknown;
        break;
    }

    return action;
}

}  // namespace hmi
}  // namespace lighting
}  // namespace body_control