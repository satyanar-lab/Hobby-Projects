#include "body_control/lighting/application/lamp_state_manager.hpp"

namespace body_control::lighting::application
{

LampStateManager::LampStateManager() noexcept
{
    Reset();
}

void LampStateManager::Reset() noexcept
{
    lamp_status_cache_[0] = {domain::LampFunction::kLeftIndicator, domain::LampOutputState::kOff, false, 0U};
    lamp_status_cache_[1] = {domain::LampFunction::kRightIndicator, domain::LampOutputState::kOff, false, 0U};
    lamp_status_cache_[2] = {domain::LampFunction::kHazardLamp, domain::LampOutputState::kOff, false, 0U};
    lamp_status_cache_[3] = {domain::LampFunction::kParkLamp, domain::LampOutputState::kOff, false, 0U};
    lamp_status_cache_[4] = {domain::LampFunction::kHeadLamp, domain::LampOutputState::kOff, false, 0U};
}

bool LampStateManager::UpdateLampStatus(
    const domain::LampStatus& lamp_status) noexcept
{
    std::size_t index {0U};

    if ((!domain::IsValidLampStatus(lamp_status)) ||
        (!TryGetIndex(lamp_status.function, index)))
    {
        return false;
    }

    lamp_status_cache_[index] = lamp_status;
    return true;
}

bool LampStateManager::GetLampStatus(
    const domain::LampFunction function,
    domain::LampStatus& lamp_status) const noexcept
{
    std::size_t index {0U};

    if (!TryGetIndex(function, index))
    {
        return false;
    }

    lamp_status = lamp_status_cache_[index];
    return true;
}

bool LampStateManager::IsFunctionActive(
    const domain::LampFunction function) const noexcept
{
    domain::LampStatus lamp_status {};

    if (!GetLampStatus(function, lamp_status))
    {
        return false;
    }

    return IsOutputStateActive(lamp_status.output_state);
}

ArbitrationContext LampStateManager::GetArbitrationContext() const noexcept
{
    ArbitrationContext context {};

    context.left_indicator_active = IsFunctionActive(domain::LampFunction::kLeftIndicator);
    context.right_indicator_active = IsFunctionActive(domain::LampFunction::kRightIndicator);
    context.hazard_lamp_active = IsFunctionActive(domain::LampFunction::kHazardLamp);
    context.park_lamp_active = IsFunctionActive(domain::LampFunction::kParkLamp);
    context.head_lamp_active = IsFunctionActive(domain::LampFunction::kHeadLamp);

    return context;
}

bool LampStateManager::TryGetIndex(
    const domain::LampFunction function,
    std::size_t& index) noexcept
{
    bool is_valid {true};

    switch (function)
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

    default:
        is_valid = false;
        break;
    }

    return is_valid;
}

bool LampStateManager::IsOutputStateActive(
    const domain::LampOutputState output_state) noexcept
{
    return (output_state == domain::LampOutputState::kOn) ||
           (output_state == domain::LampOutputState::kBlinking);
}

}  // namespace body_control::lighting::application