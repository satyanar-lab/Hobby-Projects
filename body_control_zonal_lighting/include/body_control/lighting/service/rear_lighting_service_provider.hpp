#ifndef BODY_CONTROL_LIGHTING_SERVICE_REAR_LIGHTING_SERVICE_PROVIDER_HPP
#define BODY_CONTROL_LIGHTING_SERVICE_REAR_LIGHTING_SERVICE_PROVIDER_HPP

#include "body_control/lighting/application/rear_lighting_function_manager.hpp"
#include "body_control/lighting/service/rear_lighting_service_interface.hpp"
#include "body_control/lighting/transport/transport_adapter_interface.hpp"

namespace body_control
{
namespace lighting
{
namespace service
{

class RearLightingServiceProvider final
    : public transport::TransportMessageHandlerInterface
{
public:
    RearLightingServiceProvider(
        application::RearLightingFunctionManager& rear_lighting_function_manager,
        transport::TransportAdapterInterface& transport_adapter) noexcept;

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

    static ServiceStatus ConvertTransportStatus(
        transport::TransportStatus transport_status) noexcept;

    application::RearLightingFunctionManager& rear_lighting_function_manager_;
    transport::TransportAdapterInterface& transport_adapter_;
    bool is_initialized_;
    bool is_transport_available_;
};

}  // namespace service
}  // namespace lighting
}  // namespace body_control

#endif  // BODY_CONTROL_LIGHTING_SERVICE_REAR_LIGHTING_SERVICE_PROVIDER_HPP