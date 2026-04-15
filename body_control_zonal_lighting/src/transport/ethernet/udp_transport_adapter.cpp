#include "body_control/lighting/transport/ethernet/udp_transport_adapter.hpp"

namespace body_control::lighting::transport::ethernet
{
namespace
{

[[nodiscard]] bool IsValidServiceIdentifier(
    const std::uint16_t service_id,
    const std::uint16_t instance_id) noexcept
{
    return (service_id != 0U) && (instance_id != 0U);
}

}  // namespace

UdpTransportAdapter::UdpTransportAdapter() noexcept = default;

TransportStatus UdpTransportAdapter::Initialize()
{
    is_initialized_ = true;
    return TransportStatus::kSuccess;
}

TransportStatus UdpTransportAdapter::Shutdown()
{
    if (!is_initialized_)
    {
        return TransportStatus::kNotInitialized;
    }

    is_initialized_ = false;
    is_service_offered_ = false;
    offered_service_id_ = 0U;
    offered_instance_id_ = 0U;

    return TransportStatus::kSuccess;
}

TransportStatus UdpTransportAdapter::SendRequest(
    const TransportMessage& message)
{
    if (!is_initialized_)
    {
        return TransportStatus::kNotInitialized;
    }

    if (((message.payload_length > 0U) && (message.payload_data == nullptr)) ||
        (message.service_id == 0U) ||
        (message.instance_id == 0U) ||
        (message.message_id == 0U))
    {
        return TransportStatus::kInvalidArgument;
    }

    return IsServiceAvailable(message.service_id, message.instance_id)
               ? TransportStatus::kSuccess
               : TransportStatus::kServiceUnavailable;
}

TransportStatus UdpTransportAdapter::SendResponse(
    const TransportMessage& message)
{
    if (!is_initialized_)
    {
        return TransportStatus::kNotInitialized;
    }

    if (((message.payload_length > 0U) && (message.payload_data == nullptr)) ||
        (message.service_id == 0U) ||
        (message.instance_id == 0U) ||
        (message.message_id == 0U))
    {
        return TransportStatus::kInvalidArgument;
    }

    return TransportStatus::kSuccess;
}

TransportStatus UdpTransportAdapter::SendEvent(
    const TransportMessage& message)
{
    if (!is_initialized_)
    {
        return TransportStatus::kNotInitialized;
    }

    if (((message.payload_length > 0U) && (message.payload_data == nullptr)) ||
        (message.service_id == 0U) ||
        (message.instance_id == 0U) ||
        (message.message_id == 0U))
    {
        return TransportStatus::kInvalidArgument;
    }

    return TransportStatus::kSuccess;
}

TransportStatus UdpTransportAdapter::OfferService(
    const std::uint16_t service_id,
    const std::uint16_t instance_id)
{
    if (!is_initialized_)
    {
        return TransportStatus::kNotInitialized;
    }

    if (!IsValidServiceIdentifier(service_id, instance_id))
    {
        return TransportStatus::kInvalidArgument;
    }

    offered_service_id_ = service_id;
    offered_instance_id_ = instance_id;
    is_service_offered_ = true;

    return TransportStatus::kSuccess;
}

TransportStatus UdpTransportAdapter::StopOfferService(
    const std::uint16_t service_id,
    const std::uint16_t instance_id)
{
    if (!is_initialized_)
    {
        return TransportStatus::kNotInitialized;
    }

    if (!IsValidServiceIdentifier(service_id, instance_id))
    {
        return TransportStatus::kInvalidArgument;
    }

    if ((offered_service_id_ == service_id) &&
        (offered_instance_id_ == instance_id))
    {
        is_service_offered_ = false;
        offered_service_id_ = 0U;
        offered_instance_id_ = 0U;
    }

    return TransportStatus::kSuccess;
}

bool UdpTransportAdapter::IsServiceAvailable(
    const std::uint16_t service_id,
    const std::uint16_t instance_id) const noexcept
{
    return is_service_offered_ &&
           (offered_service_id_ == service_id) &&
           (offered_instance_id_ == instance_id);
}

void UdpTransportAdapter::SetMessageHandler(
    TransportMessageHandlerInterface* message_handler) noexcept
{
    message_handler_ = message_handler;
}

}  // namespace body_control::lighting::transport::ethernet