#include "body_control/lighting/hmi/hmi_view_model.hpp"

namespace body_control
{
namespace lighting
{
namespace hmi
{

HmiViewModel::HmiViewModel() noexcept
    : lamp_statuses_ {}
    , node_health_status_ {}
{
    // Pre-seed each slot with the correct function label and a known-off output
    // state so GetLampStatus() always returns a labelled entry, even before the
    // first event arrives from the operator service.
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

void HmiViewModel::UpdateLampStatus(
    const domain::LampStatus& lamp_status) noexcept
{
    const std::size_t index = LampFunctionToIndex(lamp_status.function);

    if (index < lamp_statuses_.size())
    {
        lamp_statuses_[index] = lamp_status;
    }
}

void HmiViewModel::UpdateNodeHealthStatus(
    const domain::NodeHealthStatus& node_health_status) noexcept
{
    node_health_status_ = node_health_status;
}

bool HmiViewModel::GetLampStatus(
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

bool HmiViewModel::IsLampFunctionActive(
    const domain::LampFunction lamp_function) const noexcept
{
    const std::size_t index = LampFunctionToIndex(lamp_function);

    if (index >= lamp_statuses_.size())
    {
        return false;
    }

    return (lamp_statuses_[index].output_state == domain::LampOutputState::kOn);
}

domain::NodeHealthStatus HmiViewModel::GetNodeHealthStatus() const noexcept
{
    return node_health_status_;
}

std::size_t HmiViewModel::LampFunctionToIndex(
    const domain::LampFunction lamp_function) noexcept
{
    // SIZE_MAX sentinel (−1 cast to size_t): always >= lamp_statuses_.size()
    // so the caller's bounds check rejects kUnknown without a separate flag.
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

}  // namespace hmi
}  // namespace lighting
}  // namespace body_control