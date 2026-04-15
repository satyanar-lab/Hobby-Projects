#include "body_control/lighting/transport/vsomeip/vsomeip_client_adapter.hpp"

namespace body_control::lighting::transport::vsomeip
{

VsomeipClientAdapter::VsomeipClientAdapter(
    VsomeipRuntimeManager& runtime_manager) noexcept
    : runtime_manager_(runtime_manager)
{
}

TransportStatus VsomeipClientAdapter::Initialize()
{
    const bool runtime_initialized = runtime_manager_.Initialize();

    if (!runtime_initialized)
    {
        return TransportStatus::kTransmissionFailed;
    }

    is_initialized_ = true;
    return TransportStatus::kSuccess;
}

TransportStatus VsomeipClientAdapter::Shutdown()
{
    if (!is_initialized_)
    {
        return TransportStatus::kNotInitialized;
    }

    runtime_manager_.Shutdown();
    is_initialized_ = false;

    return TransportStatus::kSuccess;
}

TransportStatus VsomeipClientAdapter::SendRequest(
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

    return runtime_manager_.IsServiceAvailable(message.service_id, message.instance_id)
               ? TransportStatus::kSuccess
               : TransportStatus::kServiceUnavailable;
}

TransportStatus VsomeipClientAdapter::SendResponse(
    const TransportMessage& message)
{
    static_cast<void>(message);
    return TransportStatus::kTransmissionFailed;
}

TransportStatus VsomeipClientAdapter::SendEvent(
    const TransportMessage& message)
{
    static_cast<void>(message);
    return TransportStatus::kTransmissionFailed;
}

TransportStatus VsomeipClientAdapter::OfferService(
    const std::uint16_t service_id,
    const std::uint16_t instance_id)
{
    static_cast<void>(service_id);
    static_cast<void>(instance_id);
    return TransportStatus::kTransmissionFailed;
}

TransportStatus VsomeipClientAdapter::StopOfferService(
    const std::uint16_t service_id,
    const std::uint16_t instance_id)
{
    static_cast<void>(service_id);
    static_cast<void>(instance_id);
    return TransportStatus::kTransmissionFailed;
}

bool VsomeipClientAdapter::IsServiceAvailable(
    const std::uint16_t service_id,
    const std::uint16_t instance_id) const noexcept
{
    return runtime_manager_.IsServiceAvailable(service_id, instance_id);
}

void VsomeipClientAdapter::SetMessageHandler(
    TransportMessageHandlerInterface* message_handler) noexcept
{
    message_handler_ = message_handler;
}

}  // namespace body_control::lighting::transport::vsomeip