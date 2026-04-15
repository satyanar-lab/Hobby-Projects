#include "body_control/lighting/application/rear_lighting_function_manager.hpp"

namespace body_control
{
namespace lighting
{
namespace application
{

RearLightingFunctionManager::RearLightingFunctionManager() noexcept
    : lamp_statuses_ {}
{
    lamp_statuses_[0U].function = domain::LampFunction::kLeftIndicator;
    lamp_statuses_[0U].output_state = domain::LampOutputState::kOff;

    lamp_statuses_[1U].function = domain::LampFunction::kRightIndicator;
    lamp_statuses_[1U].output_state = domain::LampOutputState::kOff;

    lamp_statuses_[2U].function = domain::LampFunction::kHazardLamp;
    lamp_statuses_[2U].output_state = domain::LampOutputState::kOff;

    lamp_statuses_[3U].function = domain::LampFunction::kParkLamp;
    lamp_statuses_[3U].output_state = domain::LampOutputState::kOff;

    lamp_statuses_[4U].function = domain::LampFunction::kHeadLamp;
    lamp_statuses_[4U].output_state = domain::LampOutputState::kOff;
}

bool RearLightingFunctionManager::ApplyCommand(
    const domain::LampCommand& lamp_command) noexcept
{
    const std::size_t index = LampFunctionToIndex(lamp_command.function);

    if (index >= lamp_statuses_.size())
    {
        return false;
    }

    lamp_statuses_[index].function = lamp_command.function;
    lamp_statuses_[index].command_applied = true;
    lamp_statuses_[index].last_sequence_counter = lamp_command.sequence_counter;

    switch (lamp_command.action)
    {
    case domain::LampCommandAction::kActivate:
        lamp_statuses_[index].output_state = domain::LampOutputState::kOn;
        break;

    case domain::LampCommandAction::kDeactivate:
        lamp_statuses_[index].output_state = domain::LampOutputState::kOff;
        break;

    case domain::LampCommandAction::kToggle:
        lamp_statuses_[index].output_state =
            (lamp_statuses_[index].output_state == domain::LampOutputState::kOn)
                ? domain::LampOutputState::kOff
                : domain::LampOutputState::kOn;
        break;

    case domain::LampCommandAction::kNoAction:
    default:
        return false;
    }

    return true;
}

bool RearLightingFunctionManager::GetLampStatus(
    const domain::LampFunction lamp_function,
    domain::LampStatus& lamp_status) const noexcept
{
    const std::size_t index = LampFunctionToIndex(lamp_function);

    if (index >= lamp_statuses_.size())
    {
        return false;
    }

    lamp_status = lamp_statuses_[index];
    return true;
}

std::size_t RearLightingFunctionManager::LampFunctionToIndex(
    const domain::LampFunction lamp_function) noexcept
{
    std::size_t index {static_cast<std::size_t>(-1)};

    switch (lamp_function)
    {
    case domain::LampFunction::kLeftIndicator:
        index = 0U;
        break;

    case domain::LampFunction::kRightIndicator:
        index = 1U;
        break;

    case domain::LampFunction::kHazardLamp:
        index = 2U;
        break;

    case domain::LampFunction::kParkLamp:
        index = 3U;
        break;

    case domain::LampFunction::kHeadLamp:
        index = 4U;
        break;

    case domain::LampFunction::kUnknown:
    default:
        break;
    }

    return index;
}

}  // namespace application
}  // namespace lighting
}  // namespace body_control