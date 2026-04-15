#include "body_control/lighting/service/rear_lighting_service_consumer.hpp"

namespace body_control::lighting::service
{

RearLightingServiceConsumer::RearLightingServiceConsumer(
    transport::TransportAdapterInterface& transport_adapter) noexcept
    : transport_adapter_(transport_adapter)
    , event_listener_(nullptr)
    , is_initialized_(false)
    , is_service_available_(false)
{
}

ServiceStatus RearLightingServiceConsumer::Initialize()
{
    is_initialized_ = true;
    is_service_available_ = true;
    return ServiceStatus::kSuccess;
}

ServiceStatus RearLightingServiceConsumer::Shutdown()
{
    is_service_available_ = false;
    is_initialized_ = false;
    return ServiceStatus::kSuccess;
}

ServiceStatus RearLightingServiceConsumer::SendLampCommand(
    const domain::LampCommand& lamp_command)
{
    static_cast<void>(lamp_command);

    if (!is_initialized_)
    {
        return ServiceStatus::kNotInitialized;
    }

    return ServiceStatus::kSuccess;
}

ServiceStatus RearLightingServiceConsumer::RequestLampStatus(
    const domain::LampFunction lamp_function)
{
    static_cast<void>(lamp_function);

    if (!is_initialized_)
    {
        return ServiceStatus::kNotInitialized;
    }

    return ServiceStatus::kSuccess;
}

ServiceStatus RearLightingServiceConsumer::RequestNodeHealth()
{
    if (!is_initialized_)
    {
        return ServiceStatus::kNotInitialized;
    }

    return ServiceStatus::kSuccess;
}

void RearLightingServiceConsumer::SetEventListener(
    RearLightingServiceEventListenerInterface* const event_listener) noexcept
{
    event_listener_ = event_listener;
}

bool RearLightingServiceConsumer::IsServiceAvailable() const noexcept
{
    return is_service_available_;
}

}  // namespace body_control::lighting::service