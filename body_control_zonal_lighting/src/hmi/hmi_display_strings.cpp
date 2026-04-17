#include "body_control/lighting/hmi/hmi_display_strings.hpp"

namespace body_control::lighting::hmi
{

const char* LampFunctionToString(
    const domain::LampFunction function) noexcept
{
    switch (function)
    {
    case domain::LampFunction::kLeftIndicator:
        return "LeftIndicator";
    case domain::LampFunction::kRightIndicator:
        return "RightIndicator";
    case domain::LampFunction::kHazardLamp:
        return "HazardLamp";
    case domain::LampFunction::kParkLamp:
        return "ParkLamp";
    case domain::LampFunction::kHeadLamp:
        return "HeadLamp";
    case domain::LampFunction::kUnknown:
    default:
        return "Unknown";
    }
}

const char* LampOutputStateToString(
    const domain::LampOutputState output_state) noexcept
{
    switch (output_state)
    {
    case domain::LampOutputState::kOn:
        return "On";
    case domain::LampOutputState::kOff:
        return "Off";
    case domain::LampOutputState::kUnknown:
    default:
        return "Unknown";
    }
}

const char* NodeHealthStateToString(
    const domain::NodeHealthState health_state) noexcept
{
    switch (health_state)
    {
    case domain::NodeHealthState::kOperational:
        return "Operational";
    case domain::NodeHealthState::kDegraded:
        return "Degraded";
    case domain::NodeHealthState::kFaulted:
        return "Faulted";
    case domain::NodeHealthState::kUnavailable:
        return "Unavailable";
    case domain::NodeHealthState::kUnknown:
    default:
        return "Unknown";
    }
}

}  // namespace body_control::lighting::hmi
