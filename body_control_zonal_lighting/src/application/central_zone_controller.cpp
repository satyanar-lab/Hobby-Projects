#include "body_control/lighting/application/central_zone_controller.hpp"

#include "body_control/lighting/application/lamp_state_manager.hpp"
#include "body_control/lighting/application/node_health_monitor.hpp"
#include "body_control/lighting/service/rear_lighting_service_consumer.hpp"
#include "body_control/lighting/service/rear_lighting_service_interface.hpp"

namespace body_control::lighting::application
{
namespace
{

[[nodiscard]] ControllerStatus ConvertServiceStatusToControllerStatus(
    const service::ServiceStatus service_status) noexcept
{
    ControllerStatus controller_status {ControllerStatus::kTransmissionFailed};

    switch (service_status)
    {
    case service::ServiceStatus::kSuccess:
        controller_status = ControllerStatus::kSuccess;
        break;

    case service::ServiceStatus::kNotInitialized:
        controller_status = ControllerStatus::kNotInitialized;
        break;

    case service::ServiceStatus::kNotAvailable:
        controller_status = ControllerStatus::kServiceUnavailable;
        break;

    case service::ServiceStatus::kInvalidArgument:
        controller_status = ControllerStatus::kInvalidArgument;
        break;

    case service::ServiceStatus::kEncodeFailure:
    case service::ServiceStatus::kDecodeFailure:
    case service::ServiceStatus::kTransportFailure:
    default:
        controller_status = ControllerStatus::kTransmissionFailed;
        break;
    }

    return controller_status;
}

}  // namespace

CentralZoneController::CentralZoneController(
    service::RearLightingServiceConsumer& rear_lighting_service_consumer,
    LampStateManager& lamp_state_manager,
    NodeHealthMonitor& node_health_monitor) noexcept
    : rear_lighting_service_consumer_(rear_lighting_service_consumer),
      lamp_state_manager_(lamp_state_manager),
      node_health_monitor_(node_health_monitor)
{
}

ControllerStatus CentralZoneController::Initialize()
{
    const service::ServiceStatus service_status =
        rear_lighting_service_consumer_.Initialize();

    const ControllerStatus controller_status =
        ConvertServiceStatusToControllerStatus(service_status);

    if (controller_status == ControllerStatus::kSuccess)
    {
        is_initialized_ = true;
    }

    return controller_status;
}

ControllerStatus CentralZoneController::Shutdown()
{
    if (!is_initialized_)
    {
        return ControllerStatus::kNotInitialized;
    }

    const service::ServiceStatus service_status =
        rear_lighting_service_consumer_.Shutdown();

    const ControllerStatus controller_status =
        ConvertServiceStatusToControllerStatus(service_status);

    if (controller_status == ControllerStatus::kSuccess)
    {
        is_initialized_ = false;
    }

    return controller_status;
}

ControllerStatus CentralZoneController::SendLampCommand(
    const domain::LampFunction function,
    const domain::LampCommandAction action,
    const domain::CommandSource source)
{
    if (!is_initialized_)
    {
        return ControllerStatus::kNotInitialized;
    }

    if ((!domain::IsValidLampFunction(function)) ||
        (!domain::IsValidLampCommandAction(action)) ||
        (!domain::IsValidCommandSource(source)))
    {
        return ControllerStatus::kInvalidArgument;
    }

    if (!rear_lighting_service_consumer_.IsServiceAvailable())
    {
        return ControllerStatus::kServiceUnavailable;
    }

    const domain::LampCommand command {
        function,
        action,
        source,
        GetNextSequenceCounter()};

    const service::ServiceStatus service_status =
        rear_lighting_service_consumer_.SendLampCommand(command);

    return ConvertServiceStatusToControllerStatus(service_status);
}

ControllerStatus CentralZoneController::RequestLampStatus(
    const domain::LampFunction function)
{
    if (!is_initialized_)
    {
        return ControllerStatus::kNotInitialized;
    }

    if (!domain::IsValidLampFunction(function))
    {
        return ControllerStatus::kInvalidArgument;
    }

    if (!rear_lighting_service_consumer_.IsServiceAvailable())
    {
        return ControllerStatus::kServiceUnavailable;
    }

    const service::ServiceStatus service_status =
        rear_lighting_service_consumer_.RequestLampStatus(function);

    return ConvertServiceStatusToControllerStatus(service_status);
}

ControllerStatus CentralZoneController::RequestNodeHealth()
{
    if (!is_initialized_)
    {
        return ControllerStatus::kNotInitialized;
    }

    if (!rear_lighting_service_consumer_.IsServiceAvailable())
    {
        return ControllerStatus::kServiceUnavailable;
    }

    const service::ServiceStatus service_status =
        rear_lighting_service_consumer_.RequestNodeHealth();

    return ConvertServiceStatusToControllerStatus(service_status);
}

void CentralZoneController::OnLampStatusReceived(
    const domain::LampStatus& lamp_status)
{
    static_cast<void>(lamp_state_manager_.UpdateLampStatus(lamp_status));
}

void CentralZoneController::OnNodeHealthReceived(
    const domain::NodeHealthStatus& node_health_status)
{
    node_health_monitor_.UpdateNodeHealthStatus(node_health_status);
}

bool CentralZoneController::IsInitialized() const noexcept
{
    return is_initialized_;
}

std::uint16_t CentralZoneController::GetNextSequenceCounter() noexcept
{
    const std::uint16_t current_sequence_counter {next_sequence_counter_};

    ++next_sequence_counter_;

    if (next_sequence_counter_ == 0U)
    {
        next_sequence_counter_ = 1U;
    }

    return current_sequence_counter;
}

}  // namespace body_control::lighting::application