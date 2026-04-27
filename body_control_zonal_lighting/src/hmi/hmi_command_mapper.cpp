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
    // Key layout mirrors the operator console menu:
    //   1=Left indicator, 2=Right indicator, 3=Hazard,
    //   4=Park lamp, 5=Head lamp, 6=Node health query.
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