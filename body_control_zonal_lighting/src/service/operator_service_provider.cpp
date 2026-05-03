#include "body_control/lighting/service/operator_service_provider.hpp"

#include "body_control/lighting/transport/someip_message_builder.hpp"
#include "body_control/lighting/transport/someip_message_parser.hpp"

namespace body_control
{
namespace lighting
{
namespace service
{

OperatorServiceProvider::OperatorServiceProvider(
    application::CentralZoneController& controller,
    transport::TransportAdapterInterface& transport) noexcept
    : controller_(controller)
    , transport_(transport)
    , is_initialized_(false)
{
}

OperatorServiceStatus OperatorServiceProvider::Initialize()
{
    // Register as the controller's status observer before starting the transport
    // so no lamp or health events are missed during the transport bring-up window.
    controller_.SetStatusObserver(this);
    transport_.SetMessageHandler(this);

    const transport::TransportStatus transport_status =
        transport_.Initialize();

    if (transport_status != transport::TransportStatus::kSuccess)
    {
        // Roll back both registrations so the controller and transport are left
        // in a clean state if the transport fails to start.
        controller_.SetStatusObserver(nullptr);
        transport_.SetMessageHandler(nullptr);
        return OperatorServiceStatus::kTransportError;
    }

    is_initialized_ = true;
    return OperatorServiceStatus::kSuccess;
}

OperatorServiceStatus OperatorServiceProvider::Shutdown()
{
    controller_.SetStatusObserver(nullptr);
    transport_.SetMessageHandler(nullptr);
    static_cast<void>(transport_.Shutdown());
    is_initialized_ = false;
    return OperatorServiceStatus::kSuccess;
}

void OperatorServiceProvider::OnLampStatusReceived(
    const domain::LampStatus& lamp_status)
{
    // The controller has already updated its cache; relay immediately to clients.
    PublishLampStatusEvent(lamp_status);
}

void OperatorServiceProvider::OnNodeHealthStatusReceived(
    const domain::NodeHealthStatus& node_health_status)
{
    PublishNodeHealthEvent(node_health_status);
}

void OperatorServiceProvider::OnServiceAvailabilityChanged(
    const bool /*is_available*/)
{
    // Availability change is reflected via node health events that already
    // contain service_available and health_state fields.  No separate
    // operator-layer message is defined for pure availability notifications.
}

void OperatorServiceProvider::OnTransportMessageReceived(
    const transport::TransportMessage& transport_message)
{
    if (!is_initialized_)
    {
        return;
    }

    if (transport::SomeipMessageParser::IsOperatorLampToggleRequest(
            transport_message))
    {
        HandleLampToggleRequest(transport_message);
    }
    else if (transport::SomeipMessageParser::IsOperatorLampActivateRequest(
                 transport_message))
    {
        HandleLampActivateRequest(transport_message);
    }
    else if (transport::SomeipMessageParser::IsOperatorLampDeactivateRequest(
                 transport_message))
    {
        HandleLampDeactivateRequest(transport_message);
    }
    else if (transport::SomeipMessageParser::IsOperatorNodeHealthRequest(
                 transport_message))
    {
        HandleNodeHealthRequest();
    }
    else if (transport::SomeipMessageParser::IsOperatorInjectFaultRequest(
                 transport_message))
    {
        HandleInjectFaultRequest(transport_message);
    }
    else if (transport::SomeipMessageParser::IsOperatorClearFaultRequest(
                 transport_message))
    {
        HandleClearFaultRequest(transport_message);
    }
    else if (transport::SomeipMessageParser::IsOperatorGetFaultStatusRequest(
                 transport_message))
    {
        HandleGetFaultStatusRequest();
    }
    else
    {
        /* Intentionally ignored: unsupported operator service payload. */
    }
}

void OperatorServiceProvider::OnTransportAvailabilityChanged(
    const bool /*is_available*/)
{
    // Operator transport availability does not need to be forwarded to the
    // controller; the controller monitors the rear-lighting transport independently.
}

void OperatorServiceProvider::HandleLampToggleRequest(
    const transport::TransportMessage& transport_message)
{
    const domain::LampFunction func =
        transport::SomeipMessageParser::ParseLampFunction(transport_message);

    // kRejected is a valid outcome (e.g. indicator while hazard is active);
    // the return value is discarded because the rejection is surfaced to the HMI
    // via the absence of a subsequent lamp-status event showing a state change.
    static_cast<void>(controller_.SendLampCommand(
        func,
        domain::LampCommandAction::kToggle,
        domain::CommandSource::kHmiControlPanel));
}

void OperatorServiceProvider::HandleLampActivateRequest(
    const transport::TransportMessage& transport_message)
{
    const domain::LampFunction func =
        transport::SomeipMessageParser::ParseLampFunction(transport_message);

    static_cast<void>(controller_.SendLampCommand(
        func,
        domain::LampCommandAction::kActivate,
        domain::CommandSource::kHmiControlPanel));
}

void OperatorServiceProvider::HandleLampDeactivateRequest(
    const transport::TransportMessage& transport_message)
{
    const domain::LampFunction func =
        transport::SomeipMessageParser::ParseLampFunction(transport_message);

    static_cast<void>(controller_.SendLampCommand(
        func,
        domain::LampCommandAction::kDeactivate,
        domain::CommandSource::kHmiControlPanel));
}

void OperatorServiceProvider::HandleNodeHealthRequest()
{
    // No payload to decode; the request carries only a method ID.
    static_cast<void>(controller_.RequestNodeHealth());
}

void OperatorServiceProvider::HandleInjectFaultRequest(
    const transport::TransportMessage& transport_message)
{
    const domain::LampFunction func =
        transport::SomeipMessageParser::ParseLampFunction(transport_message);
    static_cast<void>(controller_.SendInjectFault(func));
}

void OperatorServiceProvider::HandleClearFaultRequest(
    const transport::TransportMessage& transport_message)
{
    const domain::LampFunction func =
        transport::SomeipMessageParser::ParseLampFunction(transport_message);
    static_cast<void>(controller_.SendClearFault(func));
}

void OperatorServiceProvider::HandleGetFaultStatusRequest()
{
    static_cast<void>(controller_.SendGetFaultStatus());
}

void OperatorServiceProvider::PublishLampStatusEvent(
    const domain::LampStatus& lamp_status)
{
    const transport::TransportMessage msg =
        transport::SomeipMessageBuilder::BuildOperatorLampStatusEvent(
            lamp_status);
    // Fire-and-forget; send errors are tolerated because the next controller
    // cache update will trigger another publish attempt.
    static_cast<void>(transport_.SendEvent(msg));
}

void OperatorServiceProvider::PublishNodeHealthEvent(
    const domain::NodeHealthStatus& node_health_status)
{
    const transport::TransportMessage msg =
        transport::SomeipMessageBuilder::BuildOperatorNodeHealthEvent(
            node_health_status);
    static_cast<void>(transport_.SendEvent(msg));
}

}  // namespace service
}  // namespace lighting
}  // namespace body_control
