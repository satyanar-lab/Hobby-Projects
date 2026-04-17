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
    controller_.SetStatusObserver(this);
    transport_.SetMessageHandler(this);

    const transport::TransportStatus transport_status =
        transport_.Initialize();

    if (transport_status != transport::TransportStatus::kSuccess)
    {
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
    // Availability change is reflected via node health events; no separate
    // operator-layer message defined for this phase.
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
    else
    {
        /* Intentionally ignored: unsupported operator service payload. */
    }
}

void OperatorServiceProvider::OnTransportAvailabilityChanged(
    const bool /*is_available*/)
{
}

void OperatorServiceProvider::HandleLampToggleRequest(
    const transport::TransportMessage& transport_message)
{
    const domain::LampFunction func =
        transport::SomeipMessageParser::ParseLampFunction(transport_message);

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
    static_cast<void>(controller_.RequestNodeHealth());
}

void OperatorServiceProvider::PublishLampStatusEvent(
    const domain::LampStatus& lamp_status)
{
    const transport::TransportMessage msg =
        transport::SomeipMessageBuilder::BuildOperatorLampStatusEvent(
            lamp_status);
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
