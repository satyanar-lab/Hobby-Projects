#ifndef BODY_CONTROL_LIGHTING_SERVICE_REAR_LIGHTING_SERVICE_PROVIDER_HPP_
#define BODY_CONTROL_LIGHTING_SERVICE_REAR_LIGHTING_SERVICE_PROVIDER_HPP_

#include "body_control/lighting/domain/lamp_status_types.hpp"
#include "body_control/lighting/service/rear_lighting_service_interface.hpp"

namespace body_control::lighting::application
{

class RearLightingFunctionManager;

}  // namespace body_control::lighting::application

namespace body_control::lighting::transport
{

class TransportAdapterInterface;

}  // namespace body_control::lighting::transport

namespace body_control::lighting::service
{

/**
 * @brief Node-side service provider for rear lighting functionality.
 *
 * This component connects the node-side application logic with the transport
 * adapter that exposes the service over Ethernet / SOME-IP.
 */
class RearLightingServiceProvider
{
public:
    RearLightingServiceProvider(
        application::RearLightingFunctionManager& rear_lighting_function_manager,
        transport::TransportAdapterInterface& transport_adapter) noexcept;

    RearLightingServiceProvider(const RearLightingServiceProvider&) = delete;
    RearLightingServiceProvider& operator=(const RearLightingServiceProvider&) = delete;
    RearLightingServiceProvider(RearLightingServiceProvider&&) = delete;
    RearLightingServiceProvider& operator=(RearLightingServiceProvider&&) = delete;

    ~RearLightingServiceProvider() = default;

    [[nodiscard]] ServiceStatus Initialize();
    [[nodiscard]] ServiceStatus Shutdown();

    [[nodiscard]] ServiceStatus HandleSetLampCommand(
        const domain::LampCommand& command);

    [[nodiscard]] ServiceStatus HandleGetLampStatus(
        domain::LampFunction function);

    [[nodiscard]] ServiceStatus HandleGetNodeHealth(
        const domain::NodeHealthStatus& node_health_status);

    [[nodiscard]] ServiceStatus PublishLampStatusEvent(
        const domain::LampStatus& lamp_status);

    [[nodiscard]] ServiceStatus PublishNodeHealthEvent(
        const domain::NodeHealthStatus& node_health_status);

private:
    application::RearLightingFunctionManager& rear_lighting_function_manager_;
    transport::TransportAdapterInterface& transport_adapter_;
    bool is_initialized_ {false};
};

}  // namespace body_control::lighting::service

#endif  // BODY_CONTROL_LIGHTING_SERVICE_REAR_LIGHTING_SERVICE_PROVIDER_HPP_