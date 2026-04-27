#include "body_control/lighting/service/rear_lighting_service_provider.hpp"

#include <array>

#include "body_control/lighting/domain/lighting_constants.hpp"
#include "body_control/lighting/transport/someip_message_builder.hpp"
#include "body_control/lighting/transport/someip_message_parser.hpp"

namespace body_control::lighting::service
{

// No-source constructor: synthesised health snapshot will be used.
RearLightingServiceProvider::RearLightingServiceProvider(
    application::RearLightingFunctionManager& rear_lighting_function_manager,
    transport::TransportAdapterInterface& transport_adapter) noexcept
    : rear_lighting_function_manager_(rear_lighting_function_manager)
    , transport_adapter_(transport_adapter)
    , node_health_source_(nullptr)
    , is_initialized_(false)
    , is_transport_available_(false)
{
}

// Health-source constructor: real node health will be read from source.
RearLightingServiceProvider::RearLightingServiceProvider(
    application::RearLightingFunctionManager& rear_lighting_function_manager,
    transport::TransportAdapterInterface& transport_adapter,
    application::NodeHealthSourceInterface& node_health_source) noexcept
    : rear_lighting_function_manager_(rear_lighting_function_manager)
    , transport_adapter_(transport_adapter)
    , node_health_source_(&node_health_source)
    , is_initialized_(false)
    , is_transport_available_(false)
{
}

ServiceStatus RearLightingServiceProvider::Initialize()
{
    const transport::TransportStatus transport_status =
        transport_adapter_.Initialize();

    if (transport_status != transport::TransportStatus::kSuccess)
    {
        return ConvertTransportStatus(transport_status);
    }

    // Register as handler after successful transport init; any inbound message
    // before this point would arrive on an uninitialised provider.
    transport_adapter_.SetMessageHandler(this);

    is_initialized_ = true;
    is_transport_available_ = true;

    return ServiceStatus::kSuccess;
}

ServiceStatus RearLightingServiceProvider::Shutdown()
{
    transport_adapter_.SetMessageHandler(nullptr);

    const transport::TransportStatus transport_status =
        transport_adapter_.Shutdown();

    is_initialized_ = false;
    is_transport_available_ = false;

    return ConvertTransportStatus(transport_status);
}

void RearLightingServiceProvider::BroadcastAllLampStatuses()
{
    // Iterate in a fixed order so subscribers receive statuses in a consistent
    // sequence regardless of which functions have been commanded.
    constexpr std::array<domain::LampFunction,
                         domain::system_limits::kMaximumLampFunctionCount>
        kAllFunctions {domain::LampFunction::kLeftIndicator,
                       domain::LampFunction::kRightIndicator,
                       domain::LampFunction::kHazardLamp,
                       domain::LampFunction::kParkLamp,
                       domain::LampFunction::kHeadLamp};

    for (const domain::LampFunction func : kAllFunctions)
    {
        domain::LampStatus lamp_status {};
        if (rear_lighting_function_manager_.GetLampStatus(func, lamp_status))
        {
            PublishLampStatusEvent(lamp_status);
        }
    }
}

void RearLightingServiceProvider::BroadcastNodeHealth()
{
    PublishNodeHealthEvent(BuildCurrentNodeHealthStatus());
}

void RearLightingServiceProvider::OnTransportMessageReceived(
    const transport::TransportMessage& transport_message)
{
    if (!is_initialized_)
    {
        return;
    }

    if (transport::SomeipMessageParser::IsSetLampCommandRequest(
            transport_message))
    {
        HandleSetLampCommand(transport_message);
    }
    else if (
        transport::SomeipMessageParser::IsGetLampStatusRequest(
            transport_message))
    {
        HandleGetLampStatus(transport_message);
    }
    else if (
        transport::SomeipMessageParser::IsGetNodeHealthRequest(
            transport_message))
    {
        HandleGetNodeHealth(transport_message);
    }
    else
    {
        /* Intentionally ignored: unsupported provider payload. */
    }
}

void RearLightingServiceProvider::OnTransportAvailabilityChanged(
    const bool is_available)
{
    is_transport_available_ = is_available;
}

void RearLightingServiceProvider::HandleSetLampCommand(
    const transport::TransportMessage& transport_message)
{
    const domain::LampCommand lamp_command =
        transport::SomeipMessageParser::ParseLampCommand(transport_message);

    const bool command_applied =
        rear_lighting_function_manager_.ApplyCommand(lamp_command);

    if (!command_applied)
    {
        return;
    }

    // Push the updated status immediately after applying the command (push-after-
    // write) so the controller's cache reflects the new state without waiting for
    // the next scheduled broadcast cycle.
    domain::LampStatus lamp_status {};
    const bool lamp_status_available =
        rear_lighting_function_manager_.GetLampStatus(
            lamp_command.function,
            lamp_status);

    if (lamp_status_available)
    {
        PublishLampStatusEvent(lamp_status);
    }
}

void RearLightingServiceProvider::HandleGetLampStatus(
    const transport::TransportMessage& transport_message)
{
    const domain::LampFunction lamp_function =
        transport::SomeipMessageParser::ParseLampFunction(transport_message);

    domain::LampStatus lamp_status {};
    const bool lamp_status_available =
        rear_lighting_function_manager_.GetLampStatus(
            lamp_function,
            lamp_status);

    if (lamp_status_available)
    {
        PublishLampStatusEvent(lamp_status);
    }
}

void RearLightingServiceProvider::HandleGetNodeHealth(
    const transport::TransportMessage& transport_message)
{
    // The request payload carries no data — only the method ID matters for
    // routing. Suppress the unused-parameter warning explicitly.
    static_cast<void>(transport_message);

    PublishNodeHealthEvent(BuildCurrentNodeHealthStatus());
}

void RearLightingServiceProvider::PublishLampStatusEvent(
    const domain::LampStatus& lamp_status)
{
    const transport::TransportMessage transport_message =
        transport::SomeipMessageBuilder::BuildLampStatusEvent(lamp_status);

    // Events are fire-and-forget; a send failure is logged by the transport
    // but does not affect the provider's state — the next broadcast will retry.
    static_cast<void>(transport_adapter_.SendEvent(transport_message));
}

void RearLightingServiceProvider::PublishNodeHealthEvent(
    const domain::NodeHealthStatus& node_health_status)
{
    const transport::TransportMessage transport_message =
        transport::SomeipMessageBuilder::BuildNodeHealthEvent(
            node_health_status);

    static_cast<void>(transport_adapter_.SendEvent(transport_message));
}

domain::NodeHealthStatus
RearLightingServiceProvider::BuildCurrentNodeHealthStatus() const noexcept
{
    // Authoritative source: the injected NodeHealthSource if any.
    if (node_health_source_ != nullptr)
    {
        return node_health_source_->GetNodeHealthSnapshot();
    }

    // Synthesised fallback: reflects what the provider can observe on its own
    // (transport reachability + init state).  Fault fields stay false because
    // the provider has no direct access to the lamp driver hardware without a
    // dedicated NodeHealthSource.
    domain::NodeHealthStatus synthesised {};
    synthesised.health_state =
        is_initialized_ && is_transport_available_
            ? domain::NodeHealthState::kOperational
            : domain::NodeHealthState::kDegraded;
    synthesised.ethernet_link_available  = is_transport_available_;
    synthesised.service_available        = is_initialized_;
    synthesised.lamp_driver_fault_present = false;
    synthesised.active_fault_count       = 0U;
    return synthesised;
}

ServiceStatus RearLightingServiceProvider::ConvertTransportStatus(
    const transport::TransportStatus transport_status) noexcept
{
    ServiceStatus service_status {ServiceStatus::kTransportError};

    switch (transport_status)
    {
    case transport::TransportStatus::kSuccess:
        service_status = ServiceStatus::kSuccess;
        break;

    case transport::TransportStatus::kNotInitialized:
        service_status = ServiceStatus::kNotInitialized;
        break;

    case transport::TransportStatus::kServiceUnavailable:
        service_status = ServiceStatus::kNotAvailable;
        break;

    case transport::TransportStatus::kInvalidArgument:
        service_status = ServiceStatus::kInvalidArgument;
        break;

    case transport::TransportStatus::kTransmissionFailed:
    case transport::TransportStatus::kReceptionFailed:
    default:
        service_status = ServiceStatus::kTransportError;
        break;
    }

    return service_status;
}

}  // namespace body_control::lighting::service
