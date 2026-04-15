#include "body_control/lighting/application/central_zone_controller.hpp"

namespace body_control
{
namespace lighting
{
namespace application
{

CentralZoneController::CentralZoneController(
    service::RearLightingServiceConsumerInterface& rear_lighting_service_consumer) noexcept
    : rear_lighting_service_consumer_(rear_lighting_service_consumer)
    , is_initialized_(false)
    , is_rear_node_available_(false)
    , next_sequence_counter_(1U)
    , cached_lamp_statuses_ {}
    , cached_node_health_status_ {}
{
    cached_lamp_statuses_[0U].function = domain::LampFunction::kLeftIndicator;
    cached_lamp_statuses_[1U].function = domain::LampFunction::kRightIndicator;
    cached_lamp_statuses_[2U].function = domain::LampFunction::kHazardLamp;
    cached_lamp_statuses_[3U].function = domain::LampFunction::kParkLamp;
    cached_lamp_statuses_[4U].function = domain::LampFunction::kHeadLamp;
}

ControllerStatus CentralZoneController::Initialize()
{
    rear_lighting_service_consumer_.SetEventListener(this);

    const service::ServiceStatus service_status =
        rear_lighting_service_consumer_.Initialize();

    is_initialized_ = (service_status == service::ServiceStatus::kSuccess);
    is_rear_node_available_ =
        rear_lighting_service_consumer_.IsServiceAvailable();

    return ConvertServiceStatus(service_status);
}

ControllerStatus CentralZoneController::Shutdown()
{
    rear_lighting_service_consumer_.SetEventListener(nullptr);

    const service::ServiceStatus service_status =
        rear_lighting_service_consumer_.Shutdown();

    is_initialized_ = false;
    is_rear_node_available_ = false;

    return ConvertServiceStatus(service_status);
}

ControllerStatus CentralZoneController::SendLampCommand(
    const domain::LampFunction lamp_function,
    const domain::LampCommandAction lamp_command_action,
    const domain::CommandSource command_source)
{
    if (!is_initialized_)
    {
        return ControllerStatus::kNotInitialized;
    }

    if (lamp_function == domain::LampFunction::kUnknown)
    {
        return ControllerStatus::kInvalidArgument;
    }

    if (lamp_command_action == domain::LampCommandAction::kNoAction)
    {
        return ControllerStatus::kInvalidArgument;
    }

    domain::LampCommand lamp_command {};
    lamp_command.function = lamp_function;
    lamp_command.action = lamp_command_action;
    lamp_command.source = command_source;
    lamp_command.sequence_counter = next_sequence_counter_++;

    const service::ServiceStatus service_status =
        rear_lighting_service_consumer_.SendLampCommand(lamp_command);

    return ConvertServiceStatus(service_status);
}

ControllerStatus CentralZoneController::RequestLampStatus(
    const domain::LampFunction lamp_function)
{
    if (!is_initialized_)
    {
        return ControllerStatus::kNotInitialized;
    }

    if (lamp_function == domain::LampFunction::kUnknown)
    {
        return ControllerStatus::kInvalidArgument;
    }

    const service::ServiceStatus service_status =
        rear_lighting_service_consumer_.RequestLampStatus(lamp_function);

    return ConvertServiceStatus(service_status);
}

ControllerStatus CentralZoneController::RequestNodeHealth()
{
    if (!is_initialized_)
    {
        return ControllerStatus::kNotInitialized;
    }

    const service::ServiceStatus service_status =
        rear_lighting_service_consumer_.RequestNodeHealth();

    return ConvertServiceStatus(service_status);
}

bool CentralZoneController::GetCachedLampStatus(
    const domain::LampFunction lamp_function,
    domain::LampStatus& lamp_status) const noexcept
{
    const std::size_t index = LampFunctionToIndex(lamp_function);

    if (index >= cached_lamp_statuses_.size())
    {
        return false;
    }

    lamp_status = cached_lamp_statuses_[index];
    return true;
}

domain::NodeHealthStatus CentralZoneController::GetCachedNodeHealthStatus() const noexcept
{
    return cached_node_health_status_;
}

bool CentralZoneController::IsRearNodeAvailable() const noexcept
{
    return is_rear_node_available_;
}

void CentralZoneController::OnLampStatusReceived(
    const domain::LampStatus& lamp_status)
{
    const std::size_t index = LampFunctionToIndex(lamp_status.function);

    if (index < cached_lamp_statuses_.size())
    {
        cached_lamp_statuses_[index] = lamp_status;
    }
}

void CentralZoneController::OnNodeHealthStatusReceived(
    const domain::NodeHealthStatus& node_health_status)
{
    cached_node_health_status_ = node_health_status;
}

void CentralZoneController::OnServiceAvailabilityChanged(
    const bool is_service_available)
{
    is_rear_node_available_ = is_service_available;
    cached_node_health_status_.service_available = is_service_available;
}

ControllerStatus CentralZoneController::ConvertServiceStatus(
    const service::ServiceStatus service_status) noexcept
{
    ControllerStatus controller_status {ControllerStatus::kServiceError};

    switch (service_status)
    {
    case service::ServiceStatus::kSuccess:
        controller_status = ControllerStatus::kSuccess;
        break;

    case service::ServiceStatus::kNotInitialized:
        controller_status = ControllerStatus::kNotInitialized;
        break;

    case service::ServiceStatus::kNotAvailable:
        controller_status = ControllerStatus::kNotAvailable;
        break;

    case service::ServiceStatus::kInvalidArgument:
        controller_status = ControllerStatus::kInvalidArgument;
        break;

    case service::ServiceStatus::kTransportError:
    default:
        controller_status = ControllerStatus::kServiceError;
        break;
    }

    return controller_status;
}

std::size_t CentralZoneController::LampFunctionToIndex(
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