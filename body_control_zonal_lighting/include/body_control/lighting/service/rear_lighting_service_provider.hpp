#ifndef BODY_CONTROL_LIGHTING_SERVICE_REAR_LIGHTING_SERVICE_PROVIDER_HPP_
#define BODY_CONTROL_LIGHTING_SERVICE_REAR_LIGHTING_SERVICE_PROVIDER_HPP_

#include "body_control/lighting/application/node_health_source_interface.hpp"
#include "body_control/lighting/application/rear_lighting_function_manager.hpp"
#include "body_control/lighting/service/rear_lighting_service_interface.hpp"
#include "body_control/lighting/transport/transport_adapter_interface.hpp"

namespace body_control::lighting::service
{

/**
 * @brief Rear-lighting service provider hosted by the rear node.
 *
 * Receives SetLampCommand / GetLampStatus / GetNodeHealth requests
 * from the transport and delegates lamp-control concerns to the
 * RearLightingFunctionManager, node-health reporting to an optional
 * NodeHealthSourceInterface, and publication of LampStatus /
 * NodeHealthStatus events back to the transport.
 *
 * If no NodeHealthSource is supplied, the provider synthesises a
 * minimal health snapshot from its own initialize / transport state.
 */
class RearLightingServiceProvider final
    : public transport::TransportMessageHandlerInterface
{
public:
    /**
     * @brief Construct with only the function manager (synthesised health).
     */
    RearLightingServiceProvider(
        application::RearLightingFunctionManager& rear_lighting_function_manager,
        transport::TransportAdapterInterface& transport_adapter) noexcept;

    /**
     * @brief Construct with an explicit node health source.
     *
     * @param node_health_source  Non-owning reference; must outlive the provider.
     */
    RearLightingServiceProvider(
        application::RearLightingFunctionManager& rear_lighting_function_manager,
        transport::TransportAdapterInterface& transport_adapter,
        application::NodeHealthSourceInterface& node_health_source) noexcept;

    ServiceStatus Initialize();
    ServiceStatus Shutdown();

    void OnTransportMessageReceived(
        const transport::TransportMessage& transport_message) override;

    void OnTransportAvailabilityChanged(
        bool is_available) override;

private:
    void HandleSetLampCommand(
        const transport::TransportMessage& transport_message);

    void HandleGetLampStatus(
        const transport::TransportMessage& transport_message);

    void HandleGetNodeHealth(
        const transport::TransportMessage& transport_message);

    void PublishLampStatusEvent(
        const domain::LampStatus& lamp_status);

    void PublishNodeHealthEvent(
        const domain::NodeHealthStatus& node_health_status);

    [[nodiscard]] domain::NodeHealthStatus BuildCurrentNodeHealthStatus()
        const noexcept;

    static ServiceStatus ConvertTransportStatus(
        transport::TransportStatus transport_status) noexcept;

    application::RearLightingFunctionManager& rear_lighting_function_manager_;
    transport::TransportAdapterInterface& transport_adapter_;
    application::NodeHealthSourceInterface* node_health_source_;
    bool is_initialized_;
    bool is_transport_available_;
};

}  // namespace body_control::lighting::service

#endif  // BODY_CONTROL_LIGHTING_SERVICE_REAR_LIGHTING_SERVICE_PROVIDER_HPP_
