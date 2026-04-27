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
 * Operator service provider, hosted inside the Central Zone Controller process.
 *
 * Acts as a bridge between operator clients (HMI, diagnostic console) and the
 * CentralZoneController.  Two simultaneous roles:
 *
 *   1. TransportMessageHandlerInterface — listens on the operator UDP socket
 *      for incoming RequestLampToggle / RequestLampActivate /
 *      RequestLampDeactivate / RequestNodeHealth datagrams and dispatches each
 *      to the corresponding CentralZoneController method.
 *
 *   2. RearLightingServiceEventListenerInterface — registered as the controller's
 *      status observer so it is called inline whenever the controller's lamp or
 *      health cache is updated.  It immediately re-broadcasts the update as a
 *      LampStatus or NodeHealthStatus event datagram to all connected operator
 *      clients via the operator transport.
 *
 * There is no polling loop: state changes propagate from rear node → controller
 * → this provider → operator clients entirely through callbacks.
 */
class OperatorServiceProvider final
    : public transport::TransportMessageHandlerInterface
    , public RearLightingServiceEventListenerInterface
{
public:
    /**
     * @param controller  CentralZoneController that commands are forwarded to.
     * @param transport   Operator-channel transport for sending and receiving
     *                    operator service datagrams.
     */
    OperatorServiceProvider(
        application::CentralZoneController& controller,
        transport::TransportAdapterInterface& transport) noexcept;

    /**
     * Registers this provider as the controller's status observer and
     * starts the operator transport.
     *
     * @return kSuccess or kTransportError.
     */
    [[nodiscard]] OperatorServiceStatus Initialize();

    /**
     * Deregisters the controller observer and shuts down the transport.
     *
     * @return kSuccess; never fails.
     */
    [[nodiscard]] OperatorServiceStatus Shutdown();

    // ── RearLightingServiceEventListenerInterface ────────────────────────────
    // Called by the controller after every cache update; the provider
    // immediately re-broadcasts the update to operator clients.

    /** Encodes and publishes a LampStatus event to all operator clients. */
    void OnLampStatusReceived(
        const domain::LampStatus& lamp_status) override;

    /** Encodes and publishes a NodeHealthStatus event to all operator clients. */
    void OnNodeHealthStatusReceived(
        const domain::NodeHealthStatus& node_health_status) override;

    /** Publishes a controller-availability event to all operator clients. */
    void OnServiceAvailabilityChanged(
        bool is_available) override;

    // ── TransportMessageHandlerInterface ────────────────────────────────────
    // Called by the operator transport when an operator client sends a request.

    /**
     * Routes the incoming datagram to the appropriate handle* method based
     * on its SOME/IP method ID.
     */
    void OnTransportMessageReceived(
        const transport::TransportMessage& transport_message) override;

    /** Tracks operator transport availability; logged but otherwise informational. */
    void OnTransportAvailabilityChanged(
        bool is_available) override;

private:
    /** Decodes the payload and calls controller_.SendLampCommand() with kToggle. */
    void HandleLampToggleRequest(
        const transport::TransportMessage& transport_message);

    /** Decodes the payload and calls controller_.SendLampCommand() with kActivate. */
    void HandleLampActivateRequest(
        const transport::TransportMessage& transport_message);

    /** Decodes the payload and calls controller_.SendLampCommand() with kDeactivate. */
    void HandleLampDeactivateRequest(
        const transport::TransportMessage& transport_message);

    /** Forwards a NodeHealth request to the controller without decoding a payload. */
    void HandleNodeHealthRequest();

    /** Encodes lamp_status and sends it as a SOME/IP event on the operator transport. */
    void PublishLampStatusEvent(const domain::LampStatus& lamp_status);

    /** Encodes node_health_status and sends it as a SOME/IP event on the operator transport. */
    void PublishNodeHealthEvent(const domain::NodeHealthStatus& node_health_status);

    application::CentralZoneController& controller_;
    transport::TransportAdapterInterface& transport_;
    bool is_initialized_;
};

}  // namespace service
}  // namespace lighting
}  // namespace body_control

#endif  // BODY_CONTROL_LIGHTING_SERVICE_OPERATOR_SERVICE_PROVIDER_HPP_
