#include "body_control/lighting/service/exterior_lighting_service_consumer.hpp"

#include "body_control/lighting/domain/lighting_service_ids.hpp"
#include "body_control/lighting/transport/someip_message_builder.hpp"
#include "body_control/lighting/transport/someip_message_parser.hpp"

namespace body_control
{
namespace lighting
{
namespace service
{

ExteriorLightingServiceConsumer::ExteriorLightingServiceConsumer(
    transport::TransportAdapterInterface& transport_adapter) noexcept
    : transport_adapter_(transport_adapter)
    , event_listener_(nullptr)
    , is_initialized_(false)
    , is_service_available_(false)
    // The Central Zone Controller owns this consumer; its application ID is used
    // as the SOME/IP client ID so the rear node can identify the source of requests.
    , client_id_(domain::exterior_lighting_service::kCentralZoneControllerApplicationId)
    , next_session_id_(1U)  // Session IDs start at 1; 0 is reserved per SOME/IP spec.
{
}

ServiceStatus ExteriorLightingServiceConsumer::Initialize()
{
    const transport::TransportStatus transport_status =
        transport_adapter_.Initialize();

    if (transport_status != transport::TransportStatus::kSuccess)
    {
        return ConvertTransportStatus(transport_status);
    }

    // Register as message handler only after transport init succeeds so
    // callbacks are never fired into a partially constructed state.
    transport_adapter_.SetMessageHandler(this);

    is_initialized_ = true;
    // Notify the listener immediately so its initial service-available state
    // matches the transport state at the moment Initialize() returns.
    SetServiceAvailability(true);
    return ServiceStatus::kSuccess;
}

ServiceStatus ExteriorLightingServiceConsumer::Shutdown()
{
    // Clear the handler before shutting down so no callbacks arrive during teardown.
    transport_adapter_.SetMessageHandler(nullptr);

    const transport::TransportStatus transport_status =
        transport_adapter_.Shutdown();

    is_initialized_ = false;
    is_service_available_ = false;

    return ConvertTransportStatus(transport_status);
}

ServiceStatus ExteriorLightingServiceConsumer::SendLampCommand(
    const domain::LampCommand& lamp_command)
{
    if (!is_initialized_)
    {
        return ServiceStatus::kNotInitialized;
    }

    if (!is_service_available_)
    {
        return ServiceStatus::kNotAvailable;
    }

    // Post-increment: the session ID used in this message, then advance for next.
    const transport::TransportMessage transport_message =
        transport::SomeipMessageBuilder::BuildSetLampCommandRequest(
            lamp_command,
            client_id_,
            next_session_id_++);

    const transport::TransportStatus transport_status =
        transport_adapter_.SendRequest(transport_message);

    return ConvertTransportStatus(transport_status);
}

ServiceStatus ExteriorLightingServiceConsumer::RequestLampStatus(
    const domain::LampFunction lamp_function)
{
    if (!is_initialized_)
    {
        return ServiceStatus::kNotInitialized;
    }

    if (!is_service_available_)
    {
        return ServiceStatus::kNotAvailable;
    }

    const transport::TransportMessage transport_message =
        transport::SomeipMessageBuilder::BuildGetLampStatusRequest(
            lamp_function,
            client_id_,
            next_session_id_++);

    const transport::TransportStatus transport_status =
        transport_adapter_.SendRequest(transport_message);

    return ConvertTransportStatus(transport_status);
}

ServiceStatus ExteriorLightingServiceConsumer::RequestNodeHealth()
{
    if (!is_initialized_)
    {
        return ServiceStatus::kNotInitialized;
    }

    if (!is_service_available_)
    {
        return ServiceStatus::kNotAvailable;
    }

    const transport::TransportMessage transport_message =
        transport::SomeipMessageBuilder::BuildGetNodeHealthRequest(
            client_id_,
            next_session_id_++);

    const transport::TransportStatus transport_status =
        transport_adapter_.SendRequest(transport_message);

    return ConvertTransportStatus(transport_status);
}

ServiceStatus ExteriorLightingServiceConsumer::SendInjectFault(
    const domain::LampFunction lamp_function)
{
    if (!is_initialized_) { return ServiceStatus::kNotInitialized; }
    if (!is_service_available_) { return ServiceStatus::kNotAvailable; }
    if (lamp_function == domain::LampFunction::kUnknown)
    {
        return ServiceStatus::kInvalidArgument;
    }

    const domain::FaultCommand cmd {
        lamp_function,
        domain::FaultAction::kInject,
        domain::CommandSource::kCentralZoneController,
        next_session_id_};

    const transport::TransportMessage transport_message =
        transport::SomeipMessageBuilder::BuildInjectFaultRequest(
            cmd, client_id_, next_session_id_++);

    return ConvertTransportStatus(transport_adapter_.SendRequest(transport_message));
}

ServiceStatus ExteriorLightingServiceConsumer::SendClearFault(
    const domain::LampFunction lamp_function)
{
    if (!is_initialized_) { return ServiceStatus::kNotInitialized; }
    if (!is_service_available_) { return ServiceStatus::kNotAvailable; }
    if (lamp_function == domain::LampFunction::kUnknown)
    {
        return ServiceStatus::kInvalidArgument;
    }

    const domain::FaultCommand cmd {
        lamp_function,
        domain::FaultAction::kClear,
        domain::CommandSource::kCentralZoneController,
        next_session_id_};

    const transport::TransportMessage transport_message =
        transport::SomeipMessageBuilder::BuildClearFaultRequest(
            cmd, client_id_, next_session_id_++);

    return ConvertTransportStatus(transport_adapter_.SendRequest(transport_message));
}

ServiceStatus ExteriorLightingServiceConsumer::SendGetFaultStatus()
{
    if (!is_initialized_) { return ServiceStatus::kNotInitialized; }
    if (!is_service_available_) { return ServiceStatus::kNotAvailable; }

    const transport::TransportMessage transport_message =
        transport::SomeipMessageBuilder::BuildGetFaultStatusRequest(
            client_id_, next_session_id_++);

    return ConvertTransportStatus(transport_adapter_.SendRequest(transport_message));
}

void ExteriorLightingServiceConsumer::SetEventListener(
    ExteriorLightingServiceEventListenerInterface* const event_listener) noexcept
{
    event_listener_ = event_listener;
}

bool ExteriorLightingServiceConsumer::IsServiceAvailable() const noexcept
{
    return is_service_available_;
}

void ExteriorLightingServiceConsumer::OnTransportMessageReceived(
    const transport::TransportMessage& transport_message)
{
    // Both event pushes and request responses carry lamp/health data; the same
    // payload handler works for both because the SOME/IP payload layout is
    // identical regardless of whether it arrived as an event or a response.
    // The four branches below are intentionally paired (event == request for
    // each function); clang-tidy branch-clone is suppressed.
    if (transport::SomeipMessageParser::IsLampStatusEvent(transport_message))
    { // NOLINT(bugprone-branch-clone)
        OnLampStatusPayloadReceived(transport_message);
    }
    else if (
        transport::SomeipMessageParser::IsNodeHealthEvent(transport_message))
    { // NOLINT(bugprone-branch-clone)
        OnNodeHealthPayloadReceived(transport_message);
    }
    else if (
        transport::SomeipMessageParser::IsGetLampStatusRequest(transport_message))
    {
        OnLampStatusPayloadReceived(transport_message);
    }
    else if (
        transport::SomeipMessageParser::IsGetNodeHealthRequest(transport_message))
    {
        OnNodeHealthPayloadReceived(transport_message);
    }
    else
    {
        /* Intentionally ignored: unsupported payload for consumer role. */
    }
}

void ExteriorLightingServiceConsumer::OnTransportAvailabilityChanged(
    const bool is_available)
{
    SetServiceAvailability(is_available);
}

void ExteriorLightingServiceConsumer::OnLampStatusPayloadReceived(
    const transport::TransportMessage& transport_message)
{
    if (event_listener_ == nullptr)
    {
        return;
    }

    const domain::LampStatus lamp_status =
        transport::SomeipMessageParser::ParseLampStatus(transport_message);

    event_listener_->OnLampStatusReceived(lamp_status);
}

void ExteriorLightingServiceConsumer::OnNodeHealthPayloadReceived(
    const transport::TransportMessage& transport_message)
{
    if (event_listener_ == nullptr)
    {
        return;
    }

    const domain::NodeHealthStatus node_health_status =
        transport::SomeipMessageParser::ParseNodeHealthStatus(transport_message);

    event_listener_->OnNodeHealthStatusReceived(node_health_status);
}

void ExteriorLightingServiceConsumer::SetServiceAvailability(
    const bool is_service_available) noexcept
{
    is_service_available_ = is_service_available;

    if (event_listener_ != nullptr)
    {
        event_listener_->OnServiceAvailabilityChanged(is_service_available_);
    }
}

ServiceStatus ExteriorLightingServiceConsumer::ConvertTransportStatus(
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

}  // namespace service
}  // namespace lighting
}  // namespace body_control
