#ifndef BODY_CONTROL_LIGHTING_SERVICE_OPERATOR_SERVICE_PROVIDER_HPP_
#define BODY_CONTROL_LIGHTING_SERVICE_OPERATOR_SERVICE_PROVIDER_HPP_

#include "body_control/lighting/application/central_zone_controller.hpp"
#include "body_control/lighting/service/operator_service_interface.hpp"
#include "body_control/lighting/service/rear_lighting_service_interface.hpp"
#include "body_control/lighting/transport/transport_adapter_interface.hpp"

namespace body_control
{
namespace lighting
{
namespace service
{

/**
 * @brief Operator service provider hosted inside the central zone controller.
 *
 * Bridges the CentralZoneController to operator clients (HMI, diagnostic
 * console).  Two roles in one class:
 *
 *   1. TransportMessageHandlerInterface — receives RequestLampToggle /
 *      RequestLampActivate / RequestLampDeactivate / RequestNodeHealth
 *      datagrams from operator clients and dispatches them to the
 *      CentralZoneController.
 *
 *   2. RearLightingServiceEventListenerInterface — registered as a status
 *      observer on the controller so it is notified inline whenever the
 *      controller's cache is updated; it then broadcasts LampStatus and
 *      NodeHealth events back to connected operator clients via the
 *      operator transport.
 */
class OperatorServiceProvider final
    : public transport::TransportMessageHandlerInterface
    , public RearLightingServiceEventListenerInterface
{
public:
    OperatorServiceProvider(
        application::CentralZoneController& controller,
        transport::TransportAdapterInterface& transport) noexcept;

    [[nodiscard]] OperatorServiceStatus Initialize();
    [[nodiscard]] OperatorServiceStatus Shutdown();

    // RearLightingServiceEventListenerInterface — called by the controller
    // observer hook after every cache update.
    void OnLampStatusReceived(
        const domain::LampStatus& lamp_status) override;

    void OnNodeHealthStatusReceived(
        const domain::NodeHealthStatus& node_health_status) override;

    void OnServiceAvailabilityChanged(
        bool is_available) override;

    // TransportMessageHandlerInterface — called by the operator transport
    // when an operator client sends a request.
    void OnTransportMessageReceived(
        const transport::TransportMessage& transport_message) override;

    void OnTransportAvailabilityChanged(
        bool is_available) override;

private:
    void HandleLampToggleRequest(
        const transport::TransportMessage& transport_message);

    void HandleLampActivateRequest(
        const transport::TransportMessage& transport_message);

    void HandleLampDeactivateRequest(
        const transport::TransportMessage& transport_message);

    void HandleNodeHealthRequest();

    void PublishLampStatusEvent(const domain::LampStatus& lamp_status);
    void PublishNodeHealthEvent(const domain::NodeHealthStatus& node_health_status);

    application::CentralZoneController& controller_;
    transport::TransportAdapterInterface& transport_;
    bool is_initialized_;
};

}  // namespace service
}  // namespace lighting
}  // namespace body_control

#endif  // BODY_CONTROL_LIGHTING_SERVICE_OPERATOR_SERVICE_PROVIDER_HPP_
