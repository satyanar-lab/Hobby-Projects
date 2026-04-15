#include "body_control/lighting/hmi/hmi_command_mapper.hpp"

namespace body_control::lighting::hmi
{

bool HmiCommandMapper::MapActionToLampCommand(
    const HmiAction action,
    const bool current_active_state,
    HmiLampCommandRequest& command_request) const noexcept
{
    bool is_supported_action {true};

    command_request.function = domain::LampFunction::kUnknown;
    command_request.action = domain::LampCommandAction::kUnknown;

    switch (action)
    {
    case HmiAction::kToggleLeftIndicator:
        command_request.function = domain::LampFunction::kLeftIndicator;
        break;

    case HmiAction::kToggleRightIndicator:
        command_request.function = domain::LampFunction::kRightIndicator;
        break;

    case HmiAction::kToggleHazardLamp:
        command_request.function = domain::LampFunction::kHazardLamp;
        break;

    case HmiAction::kToggleParkLamp:
        command_request.function = domain::LampFunction::kParkLamp;
        break;

    case HmiAction::kToggleHeadLamp:
        command_request.function = domain::LampFunction::kHeadLamp;
        break;

    case HmiAction::kRequestNodeHealth:
    case HmiAction::kUnknown:
    default:
        is_supported_action = false;
        break;
    }

    if (!is_supported_action)
    {
        return false;
    }

    command_request.action =
        current_active_state ? domain::LampCommandAction::kDeactivate
                             : domain::LampCommandAction::kActivate;

    return true;
}

bool HmiCommandMapper::IsLampControlAction(
    const HmiAction action) const noexcept
{
    return (action == HmiAction::kToggleLeftIndicator) ||
           (action == HmiAction::kToggleRightIndicator) ||
           (action == HmiAction::kToggleHazardLamp) ||
           (action == HmiAction::kToggleParkLamp) ||
           (action == HmiAction::kToggleHeadLamp);
}

}  // namespace body_control::lighting::hmi