#ifndef BODY_CONTROL_LIGHTING_SERVICE_REAR_LIGHTING_SERVICE_INTERFACE_HPP_
#define BODY_CONTROL_LIGHTING_SERVICE_REAR_LIGHTING_SERVICE_INTERFACE_HPP_

#include <cstdint>

#include "body_control/lighting/domain/lamp_command_types.hpp"
#include "body_control/lighting/domain/lamp_status_types.hpp"

namespace body_control::lighting::service
{

/**
 * @brief Result status returned by service-layer operations.
 */
enum class ServiceStatus : std::uint8_t
{
    kSuccess = 0U,
    kNotInitialized = 1U,
    kNotAvailable = 2U,
    kInvalidArgument = 3U,
    kEncodeFailure = 4U,
    kDecodeFailure = 5U,
    kTransportFailure = 6U
};

/**
 * @brief Callback interface used by the service consumer to notify the
 *        application layer when asynchronous data arrives.
 */
class RearLightingServiceEventListenerInterface
{
public:
    virtual ~RearLightingServiceEventListenerInterface() = default;

    virtual void OnLampStatusEvent(
        const domain::LampStatus& lamp_status) = 0;

    virtual void OnNodeHealthEvent(
        const domain::NodeHealthStatus& node_health_status) = 0;
};

/**
 * @brief Application-facing service consumer interface.
 *
 * This interface abstracts the underlying transport implementation from the
 * Central Zone Controller.
 */
class RearLightingServiceConsumerInterface
{
public:
    virtual ~RearLightingServiceConsumerInterface() = default;

    [[nodiscard]] virtual ServiceStatus Initialize() = 0;
    [[nodiscard]] virtual ServiceStatus Shutdown() = 0;

    [[nodiscard]] virtual ServiceStatus SendLampCommand(
        const domain::LampCommand& command) = 0;

    [[nodiscard]] virtual ServiceStatus RequestLampStatus(
        domain::LampFunction function) = 0;

    [[nodiscard]] virtual ServiceStatus RequestNodeHealth() = 0;

    [[nodiscard]] virtual bool IsServiceAvailable() const noexcept = 0;

    virtual void SetEventListener(
        RearLightingServiceEventListenerInterface* event_listener) noexcept = 0;
};

}  // namespace body_control::lighting::service

#endif  // BODY_CONTROL_LIGHTING_SERVICE_REAR_LIGHTING_SERVICE_INTERFACE_HPP_