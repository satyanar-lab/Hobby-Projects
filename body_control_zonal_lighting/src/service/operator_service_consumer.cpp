#include "body_control/lighting/service/operator_service_consumer.hpp"

#include "body_control/lighting/transport/someip_message_builder.hpp"
#include "body_control/lighting/transport/someip_message_parser.hpp"

namespace body_control
{
namespace lighting
{
namespace service
{

OperatorServiceConsumer::OperatorServiceConsumer(
    transport::TransportAdapterInterface& transport,
    const std::uint16_t client_id) noexcept
    : transport_(transport)
    , listener_(nullptr)
    , is_initialized_(false)
    , client_id_(client_id)
    , next_session_id_(1U)  // Session IDs start at 1; 0 is reserved per SOME/IP spec.
    , cached_lamp_statuses_ {}
    , cached_node_health_status_ {}
{
}

OperatorServiceStatus OperatorServiceConsumer::Initialize()
{
    // Register the message handler before calling transport Initialize so that
    // any availability callback fired during init is not lost.
    transport_.SetMessageHandler(this);

    const transport::TransportStatus transport_status =
        transport_.Initialize();

    if (transport_status != transport::TransportStatus::kSuccess)
    {
        transport_.SetMessageHandler(nullptr);
        return OperatorServiceStatus::kTransportError;
    }

    is_initialized_ = true;
    return OperatorServiceStatus::kSuccess;
}

OperatorServiceStatus OperatorServiceConsumer::Shutdown()
{
    transport_.SetMessageHandler(nullptr);
    static_cast<void>(transport_.Shutdown());
    is_initialized_ = false;
    return OperatorServiceStatus::kSuccess;
}

OperatorServiceStatus OperatorServiceConsumer::RequestLampToggle(
    const domain::LampFunction lamp_function)
{
    const transport::TransportMessage msg =
        transport::SomeipMessageBuilder::BuildOperatorLampToggleRequest(
            lamp_function,
            client_id_,
            next_session_id_++);

    return SendRequest(msg);
}

OperatorServiceStatus OperatorServiceConsumer::RequestLampActivate(
    const domain::LampFunction lamp_function)
{
    const transport::TransportMessage msg =
        transport::SomeipMessageBuilder::BuildOperatorLampActivateRequest(
            lamp_function,
            client_id_,
            next_session_id_++);

    return SendRequest(msg);
}

OperatorServiceStatus OperatorServiceConsumer::RequestLampDeactivate(
    const domain::LampFunction lamp_function)
{
    const transport::TransportMessage msg =
        transport::SomeipMessageBuilder::BuildOperatorLampDeactivateRequest(
            lamp_function,
            client_id_,
            next_session_id_++);

    return SendRequest(msg);
}

OperatorServiceStatus OperatorServiceConsumer::RequestNodeHealth()
{
    const transport::TransportMessage msg =
        transport::SomeipMessageBuilder::BuildOperatorNodeHealthRequest(
            client_id_,
            next_session_id_++);

    return SendRequest(msg);
}

OperatorServiceStatus OperatorServiceConsumer::RequestInjectFault(
    const domain::LampFunction lamp_function)
{
    const transport::TransportMessage msg =
        transport::SomeipMessageBuilder::BuildOperatorInjectFaultRequest(
            lamp_function, client_id_, next_session_id_++);
    return SendRequest(msg);
}

OperatorServiceStatus OperatorServiceConsumer::RequestClearFault(
    const domain::LampFunction lamp_function)
{
    const transport::TransportMessage msg =
        transport::SomeipMessageBuilder::BuildOperatorClearFaultRequest(
            lamp_function, client_id_, next_session_id_++);
    return SendRequest(msg);
}

OperatorServiceStatus OperatorServiceConsumer::RequestGetFaultStatus()
{
    const transport::TransportMessage msg =
        transport::SomeipMessageBuilder::BuildOperatorGetFaultStatusRequest(
            client_id_, next_session_id_++);
    return SendRequest(msg);
}

bool OperatorServiceConsumer::GetLampStatus(
    const domain::LampFunction lamp_function,
    domain::LampStatus& lamp_status) const noexcept
{
    const std::size_t index = LampFunctionToIndex(lamp_function);
    if (index >= kMaxLampFunctions)
    {
        return false;
    }
    lamp_status = cached_lamp_statuses_[index];
    return true;
}

void OperatorServiceConsumer::GetNodeHealthStatus(
    domain::NodeHealthStatus& node_health_status) const noexcept
{
    node_health_status = cached_node_health_status_;
}

void OperatorServiceConsumer::SetEventListener(
    OperatorServiceEventListenerInterface* const listener) noexcept
{
    listener_ = listener;
}

void OperatorServiceConsumer::OnTransportMessageReceived(
    const transport::TransportMessage& transport_message)
{
    if (transport::SomeipMessageParser::IsOperatorLampStatusEvent(
            transport_message))
    {
        const domain::LampStatus lamp_status =
            transport::SomeipMessageParser::ParseLampStatus(transport_message);

        // Update the local cache first so a GetLampStatus() call inside the
        // listener callback already sees the new value.
        const std::size_t index = LampFunctionToIndex(lamp_status.function);
        if (index < kMaxLampFunctions)
        {
            cached_lamp_statuses_[index] = lamp_status;
        }

        if (listener_ != nullptr)
        {
            listener_->OnLampStatusUpdated(lamp_status);
        }
    }
    else if (transport::SomeipMessageParser::IsOperatorNodeHealthEvent(
                 transport_message))
    {
        const domain::NodeHealthStatus node_health_status =
            transport::SomeipMessageParser::ParseNodeHealthStatus(
                transport_message);

        cached_node_health_status_ = node_health_status;

        if (listener_ != nullptr)
        {
            listener_->OnNodeHealthUpdated(node_health_status);
        }
    }
    else
    {
        /* Intentionally ignored: unsupported operator service payload. */
    }
}

void OperatorServiceConsumer::OnTransportAvailabilityChanged(
    const bool is_available)
{
    if (listener_ != nullptr)
    {
        listener_->OnControllerAvailabilityChanged(is_available);
    }
}

OperatorServiceStatus OperatorServiceConsumer::SendRequest(
    const transport::TransportMessage& msg)
{
    if (!is_initialized_)
    {
        return OperatorServiceStatus::kNotInitialized;
    }

    const transport::TransportStatus transport_status =
        transport_.SendRequest(msg);

    return ConvertTransportStatus(transport_status);
}

std::size_t OperatorServiceConsumer::LampFunctionToIndex(
    const domain::LampFunction lamp_function) noexcept
{
    // LampFunction enum values start at 1 (kUnknown = 0); subtract 1 to map
    // to a zero-based array index.  kUnknown (raw == 0) and any value above
    // kMaxLampFunctions return the sentinel kMaxLampFunctions, which is always
    // >= kMaxLampFunctions so the caller's bounds check rejects it.
    const auto raw = static_cast<std::size_t>(lamp_function);
    if ((raw == 0U) || (raw > kMaxLampFunctions))
    {
        return kMaxLampFunctions;
    }
    return raw - 1U;
}

OperatorServiceStatus OperatorServiceConsumer::ConvertTransportStatus(
    const transport::TransportStatus transport_status) noexcept
{
    OperatorServiceStatus status {OperatorServiceStatus::kTransportError};

    switch (transport_status)
    {
    case transport::TransportStatus::kSuccess:
        status = OperatorServiceStatus::kSuccess;
        break;

    case transport::TransportStatus::kNotInitialized:
        status = OperatorServiceStatus::kNotInitialized;
        break;

    case transport::TransportStatus::kServiceUnavailable:
        status = OperatorServiceStatus::kNotAvailable;
        break;

    case transport::TransportStatus::kInvalidArgument:
        status = OperatorServiceStatus::kInvalidArgument;
        break;

    case transport::TransportStatus::kTransmissionFailed:
    case transport::TransportStatus::kReceptionFailed:
    default:
        status = OperatorServiceStatus::kTransportError;
        break;
    }

    return status;
}

}  // namespace service
}  // namespace lighting
}  // namespace body_control
