#ifndef BODY_CONTROL_LIGHTING_SERVICE_REAR_LIGHTING_SERVICE_CONSUMER_HPP
#define BODY_CONTROL_LIGHTING_SERVICE_REAR_LIGHTING_SERVICE_CONSUMER_HPP

#include <cstdint>

#include "body_control/lighting/service/rear_lighting_service_interface.hpp"
#include "body_control/lighting/transport/transport_adapter_interface.hpp"

namespace body_control
{
namespace lighting
{
namespace service
{

/**
 * Transport-backed implementation of the rear-lighting service consumer.
 *
 * Runs inside the Central Zone Controller process.  Serialises commands into
 * SOME/IP-framed UDP datagrams via the TransportAdapterInterface, and
 * demultiplexes incoming datagrams into LampStatus and NodeHealthStatus
 * callbacks on the registered event listener.
 *
 * The consumer implements TransportMessageHandlerInterface so the transport
 * adapter can call back on the same object when a message arrives, keeping
 * the call path symmetric and avoiding a separate dispatcher object.
 */
class RearLightingServiceConsumer final
    : public RearLightingServiceConsumerInterface
    , public transport::TransportMessageHandlerInterface
{
public:
    /**
     * Stores references to the transport adapter.  Does not open any socket.
     *
     * @param transport_adapter  Must outlive the consumer; used for all sends and receives.
     */
    explicit RearLightingServiceConsumer(
        transport::TransportAdapterInterface& transport_adapter) noexcept;

    /** Starts the transport and subscribes to lamp-status and health events. */
    ServiceStatus Initialize() override;

    /** Shuts down the transport and clears the event listener. */
    ServiceStatus Shutdown() override;

    /** Encodes lamp_command and sends it as a SetLampCommand SOME/IP method call. */
    ServiceStatus SendLampCommand(
        const domain::LampCommand& lamp_command) override;

    /** Sends a GetLampStatus request for the named function. */
    ServiceStatus RequestLampStatus(
        domain::LampFunction lamp_function) override;

    /** Sends a GetNodeHealth request. */
    ServiceStatus RequestNodeHealth() override;

    /** Sends an InjectFault method call to the rear node for the given function. */
    ServiceStatus SendInjectFault(
        domain::LampFunction lamp_function) override;

    /** Sends a ClearFault method call to the rear node for the given function. */
    ServiceStatus SendClearFault(
        domain::LampFunction lamp_function) override;

    /** Sends a GetFaultStatus request to the rear node. */
    ServiceStatus SendGetFaultStatus() override;

    /** Registers the listener that receives decoded event callbacks. */
    void SetEventListener(
        RearLightingServiceEventListenerInterface* event_listener) noexcept override;

    /** Returns true when the rear-lighting service is offered on the transport. */
    bool IsServiceAvailable() const noexcept override;

    /**
     * Entry point for all inbound datagrams from the transport adapter.
     *
     * Inspects the SOME/IP event ID and routes to the appropriate handler
     * (lamp-status or node-health); unknown message IDs are silently dropped.
     */
    void OnTransportMessageReceived(
        const transport::TransportMessage& transport_message) override;

    /** Mirrors transport availability to the event listener as a service-availability change. */
    void OnTransportAvailabilityChanged(
        bool is_available) override;

private:
    /** Decodes a lamp-status payload and fires OnLampStatusReceived() on the listener. */
    void OnLampStatusPayloadReceived(
        const transport::TransportMessage& transport_message);

    /** Decodes a node-health payload and fires OnNodeHealthStatusReceived() on the listener. */
    void OnNodeHealthPayloadReceived(
        const transport::TransportMessage& transport_message);

    /** Updates the cached availability flag and notifies the listener. */
    void SetServiceAvailability(
        bool is_service_available) noexcept;

    /** Maps a TransportStatus to the equivalent ServiceStatus. */
    static ServiceStatus ConvertTransportStatus(
        transport::TransportStatus transport_status) noexcept;

    transport::TransportAdapterInterface& transport_adapter_;

    /// Non-owning pointer; may be nullptr if no listener is registered.
    RearLightingServiceEventListenerInterface* event_listener_;

    bool is_initialized_;
    bool is_service_available_;

    /// Identifies this consumer in SOME/IP message headers sent to the rear node.
    std::uint16_t client_id_;

    /// Monotonically increasing SOME/IP session ID, incremented per request.
    std::uint16_t next_session_id_;
};

}  // namespace service
}  // namespace lighting
}  // namespace body_control

#endif  // BODY_CONTROL_LIGHTING_SERVICE_REAR_LIGHTING_SERVICE_CONSUMER_HPP
