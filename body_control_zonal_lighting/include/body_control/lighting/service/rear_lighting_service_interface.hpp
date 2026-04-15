#ifndef BODY_CONTROL_LIGHTING_SERVICE_REAR_LIGHTING_SERVICE_INTERFACE_HPP
#define BODY_CONTROL_LIGHTING_SERVICE_REAR_LIGHTING_SERVICE_INTERFACE_HPP

#include "body_control/lighting/domain/lamp_command_types.hpp"
#include "body_control/lighting/domain/lamp_status_types.hpp"

namespace body_control
{
namespace lighting
{
namespace service
{

enum class ServiceStatus : std::uint8_t
{
    kSuccess = 0U,
    kNotInitialized = 1U,
    kNotAvailable = 2U,
    kInvalidArgument = 3U,
    kTransportError = 4U
};

class RearLightingServiceEventListenerInterface
{
public:
    virtual ~RearLightingServiceEventListenerInterface() = default;

    virtual void OnLampStatusReceived(
        const domain::LampStatus& lamp_status) = 0;

    virtual void OnNodeHealthStatusReceived(
        const domain::NodeHealthStatus& node_health_status) = 0;

    virtual void OnServiceAvailabilityChanged(
        bool is_service_available) = 0;
};

class RearLightingServiceConsumerInterface
{
public:
    virtual ~RearLightingServiceConsumerInterface() = default;

    virtual ServiceStatus Initialize() = 0;
    virtual ServiceStatus Shutdown() = 0;

    virtual ServiceStatus SendLampCommand(
        const domain::LampCommand& lamp_command) = 0;

    virtual ServiceStatus RequestLampStatus(
        domain::LampFunction lamp_function) = 0;

    virtual ServiceStatus RequestNodeHealth() = 0;

    virtual void SetEventListener(
        RearLightingServiceEventListenerInterface* event_listener) noexcept = 0;

    virtual bool IsServiceAvailable() const noexcept = 0;
};

}  // namespace service
}  // namespace lighting
}  // namespace body_control

#endif  // BODY_CONTROL_LIGHTING_SERVICE_REAR_LIGHTING_SERVICE_INTERFACE_HPP