#include "body_control/lighting/hmi/hmi_view_model.hpp"

namespace body_control::lighting::hmi
{
namespace
{

[[nodiscard]] bool IsOutputStateActive(
    const domain::LampOutputState output_state) noexcept
{
    return (output_state == domain::LampOutputState::kOn) ||
           (output_state == domain::LampOutputState::kBlinking);
}

}  // namespace

void HmiViewModel::Reset() noexcept
{
    left_indicator_active_ = false;
    right_indicator_active_ = false;
    hazard_lamp_active_ = false;
    park_lamp_active_ = false;
    head_lamp_active_ = false;
    node_health_status_ = {};
}

void HmiViewModel::UpdateLampStatus(
    const domain::LampStatus& lamp_status) noexcept
{
    if (!domain::IsValidLampStatus(lamp_status))
    {
        return;
    }

    const bool is_active {IsOutputStateActive(lamp_status.output_state)};

    switch (lamp_status.function)
    {
    case domain::LampFunction::kLeftIndicator:
        left_indicator_active_ = is_active;
        break;

    case domain::LampFunction::kRightIndicator:
        right_indicator_active_ = is_active;
        break;

    case domain::LampFunction::kHazardLamp:
        hazard_lamp_active_ = is_active;
        break;

    case domain::LampFunction::kParkLamp:
        park_lamp_active_ = is_active;
        break;

    case domain::LampFunction::kHeadLamp:
        head_lamp_active_ = is_active;
        break;

    default:
        break;
    }
}

void HmiViewModel::UpdateNodeHealthStatus(
    const domain::NodeHealthStatus& node_health_status) noexcept
{
    if (!domain::IsValidNodeHealthStatus(node_health_status))
    {
        return;
    }

    node_health_status_ = node_health_status;
}

bool HmiViewModel::IsLampFunctionActive(
    const domain::LampFunction function) const noexcept
{
    bool is_active {false};

    switch (function)
    {
    case domain::LampFunction::kLeftIndicator:
        is_active = left_indicator_active_;
        break;

    case domain::LampFunction::kRightIndicator:
        is_active = right_indicator_active_;
        break;

    case domain::LampFunction::kHazardLamp:
        is_active = hazard_lamp_active_;
        break;

    case domain::LampFunction::kParkLamp:
        is_active = park_lamp_active_;
        break;

    case domain::LampFunction::kHeadLamp:
        is_active = head_lamp_active_;
        break;

    default:
        is_active = false;
        break;
    }

    return is_active;
}

domain::NodeHealthStatus HmiViewModel::GetNodeHealthStatus() const noexcept
{
    return node_health_status_;
}

}  // namespace body_control::lighting::hmi