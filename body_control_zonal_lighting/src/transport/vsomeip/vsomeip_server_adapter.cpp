#include "body_control/lighting/transport/vsomeip/vsomeip_server_adapter.hpp"

namespace body_control::lighting::transport::vsomeip
{

VsomeipServerAdapter::VsomeipServerAdapter(
    VsomeipRuntimeManager& runtime_manager) noexcept
    : runtime_manager_(runtime_manager)
{
}

TransportStatus VsomeipServerAdapter::Initialize()
{
    const bool runtime_initialized = runtime_manager_.Initialize();

    if (!runtime_initialized)
    {
        return TransportStatus::kTransmissionFailed;
    }

    is_initialized_ = true;
    return TransportStatus::kSuccess;
}

TransportStatus VsomeipServerAdapter::Shutdown()
{
    if (!is_initialized_)
    {
        return TransportStatus::kNotInitialized;
    }

    if (is_service_offered_)
    {
        runtime_manager_.SetServiceAvailability(
            offered_service_id_,
            offered_instance_id_,
            false);
    }

    runtime_manager_.Shutdown();
    is_initialized_ = false;
    is_service_offered_ = false;
    offered_service_id_ = 0U;
    offered_instance_id_ = 0U;

    return TransportStatus::kSuccess;
}

TransportStatus VsomeipServerAdapter::SendRequest(
    const TransportMessage& message)
{
    static_cast<void>(message);
    return TransportStatus::kTransmissionFailed;
}

TransportStatus VsomeipServerAdapter::SendResponse(
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

TransportStatus VsomeipServerAdapter::SendEvent(
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

TransportStatus VsomeipServerAdapter::OfferService(
    const std::uint16_t service_id,
    const std::uint16_t instance_id)
{
    if (!is_initialized_)
    {
        return TransportStatus::kNotInitialized;
    }

    if ((service_id == 0U) || (instance_id == 0U))
    {
        return TransportStatus::kInvalidArgument;
    }

    offered_service_id_ = service_id;
    offered_instance_id_ = instance_id;
    is_service_offered_ = true;

    runtime_manager_.SetServiceAvailability(service_id, instance_id, true);

    return TransportStatus::kSuccess;
}

TransportStatus VsomeipServerAdapter::StopOfferService(
    const std::uint16_t service_id,
    const std::uint16_t instance_id)
{
    if (!is_initialized_)
    {
        return TransportStatus::kNotInitialized;
    }

    if ((service_id == 0U) || (instance_id == 0U))
    {
        return TransportStatus::kInvalidArgument;
    }

    if ((offered_service_id_ == service_id) &&
        (offered_instance_id_ == instance_id))
    {
        runtime_manager_.SetServiceAvailability(service_id, instance_id, false);
        is_service_offered_ = false;
        offered_service_id_ = 0U;
        offered_instance_id_ = 0U;
    }

    return TransportStatus::kSuccess;
}

bool VsomeipServerAdapter::IsServiceAvailable(
    const std::uint16_t service_id,
    const std::uint16_t instance_id) const noexcept
{
    return runtime_manager_.IsServiceAvailable(service_id, instance_id);
}

void VsomeipServerAdapter::SetMessageHandler(
    TransportMessageHandlerInterface* message_handler) noexcept
{
    message_handler_ = message_handler;
}

}  // namespace body_control::lighting::transport::vsomeip