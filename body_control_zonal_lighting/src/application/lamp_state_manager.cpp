#include "body_control/lighting/application/lamp_state_manager.hpp"

namespace body_control
{
namespace lighting
{
namespace application
{

LampStateManager::LampStateManager() noexcept
{
    Reset();
}

void LampStateManager::Reset() noexcept
{
    for (std::size_t index = 0U; index < kManagedLampCount; ++index)
    {
        lamp_status_cache_[index] = domain::LampStatus {};
    }

    // Pre-seed each slot with the correct LampFunction so that a GetLampStatus()
    // call before any status has been received returns a zero-initialised entry
    // with the right function field rather than kUnknown.
    lamp_status_cache_[0U].function = domain::LampFunction::kLeftIndicator;
    lamp_status_cache_[1U].function = domain::LampFunction::kRightIndicator;
    lamp_status_cache_[2U].function = domain::LampFunction::kHazardLamp;
    lamp_status_cache_[3U].function = domain::LampFunction::kParkLamp;
    lamp_status_cache_[4U].function = domain::LampFunction::kHeadLamp;
}

bool LampStateManager::UpdateLampStatus(
    const domain::LampStatus& lamp_status) noexcept
{
    std::size_t index {0U};

    if (!TryGetIndex(lamp_status.function, index))
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
    context.left_indicator_active =
        IsFunctionActive(domain::LampFunction::kLeftIndicator);
    context.right_indicator_active =
        IsFunctionActive(domain::LampFunction::kRightIndicator);
    context.hazard_lamp_active =
        IsFunctionActive(domain::LampFunction::kHazardLamp);
    context.park_lamp_active =
        IsFunctionActive(domain::LampFunction::kParkLamp);
    context.head_lamp_active =
        IsFunctionActive(domain::LampFunction::kHeadLamp);
    return context;
}

bool LampStateManager::TryGetIndex(
    const domain::LampFunction function,
    std::size_t& index) noexcept
{
    // Switch covers every named enumerator; kUnknown and any future undefined
    // value fall through to default and return false.
    switch (function)
    {
    case domain::LampFunction::kLeftIndicator:
        index = 0U;
        return true;

    case domain::LampFunction::kRightIndicator:
        index = 1U;
        return true;

    case domain::LampFunction::kHazardLamp:
        index = 2U;
        return true;

    case domain::LampFunction::kParkLamp:
        index = 3U;
        return true;

    case domain::LampFunction::kHeadLamp:
        index = 4U;
        return true;

    case domain::LampFunction::kUnknown:
    default:
        break;
    }

    index = 0U;
    return false;
}

bool LampStateManager::IsOutputStateActive(
    const domain::LampOutputState output_state) noexcept
{
    // kUnknown deliberately returns false — unknown state is treated as off
    // for arbitration purposes to avoid false conflict detections on startup.
    return output_state == domain::LampOutputState::kOn;
}

}  // namespace application
}  // namespace lighting
}  // namespace body_control
