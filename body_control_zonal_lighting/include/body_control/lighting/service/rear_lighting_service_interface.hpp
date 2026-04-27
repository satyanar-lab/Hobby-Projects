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

/**
 * Return status for rear-lighting service operations.
 *
 * Returned by the consumer interface methods so callers can handle transport
 * failures, uninitialized state, and argument errors without exceptions.
 */
enum class ServiceStatus : std::uint8_t
{
    kSuccess         = 0U,  ///< Operation sent or processed without error.
    kNotInitialized  = 1U,  ///< Initialize() has not yet been called.
    kNotAvailable    = 2U,  ///< Rear lighting node is not currently reachable.
    kInvalidArgument = 3U,  ///< A domain type field is out of its valid range.
    kTransportError  = 4U,  ///< The underlying transport layer reported a send failure.
};

/**
 * Event callbacks delivered to whoever registered with SetEventListener().
 *
 * In production the CentralZoneController implements this interface so it
 * receives lamp-state and health updates as the rear node publishes them.
 * The OperatorServiceProvider also implements it to relay those updates
 * onward to connected HMI clients.
 */
class RearLightingServiceEventListenerInterface
{
public:
    virtual ~RearLightingServiceEventListenerInterface() = default;

    /** Called when the rear node publishes a LampStatus event or responds to a status request. */
    virtual void OnLampStatusReceived(
        const domain::LampStatus& lamp_status) = 0;

    /** Called when the rear node publishes a NodeHealthStatus event or responds to a health request. */
    virtual void OnNodeHealthStatusReceived(
        const domain::NodeHealthStatus& node_health_status) = 0;

    /**
     * Called when the transport layer detects that the rear-lighting
     * vsomeip service has appeared or disappeared on the network.
     *
     * @param is_service_available  true = service offered; false = service gone.
     */
    virtual void OnServiceAvailabilityChanged(
        bool is_service_available) = 0;
};

/**
 * Abstract interface for the rear-lighting service consumer.
 *
 * Implemented by RearLightingServiceConsumer (vsomeip/UDP transport) and
 * potentially by a mock in unit tests.  The CentralZoneController holds a
 * reference to this interface, keeping it transport-agnostic.
 */
class RearLightingServiceConsumerInterface
{
public:
    virtual ~RearLightingServiceConsumerInterface() = default;

    /**
     * Connects to the transport, subscribes to lamp-status and health events,
     * and registers the event listener.  Must be called before any other method.
     *
     * @return kSuccess or kTransportError if the transport fails to initialise.
     */
    virtual ServiceStatus Initialize() = 0;

    /**
     * Unsubscribes from all events and releases transport resources.
     *
     * @return kSuccess; never fails.
     */
    virtual ServiceStatus Shutdown() = 0;

    /**
     * Serialises and sends a LampCommand to the rear node.
     *
     * @param lamp_command  Must pass IsValidLampCommand(); otherwise kInvalidArgument.
     * @return kSuccess, kNotInitialized, kNotAvailable, kInvalidArgument, or kTransportError.
     */
    virtual ServiceStatus SendLampCommand(
        const domain::LampCommand& lamp_command) = 0;

    /**
     * Sends a GetLampStatus request for the given function.
     * The response arrives asynchronously via OnLampStatusReceived().
     *
     * @param lamp_function  Target function; kUnknown returns kInvalidArgument.
     */
    virtual ServiceStatus RequestLampStatus(
        domain::LampFunction lamp_function) = 0;

    /**
     * Sends a GetNodeHealth request to the rear node.
     * The response arrives asynchronously via OnNodeHealthStatusReceived().
     */
    virtual ServiceStatus RequestNodeHealth() = 0;

    /**
     * Registers the callback object that receives event notifications.
     * Pass nullptr to deregister.  Only one listener is supported.
     */
    virtual void SetEventListener(
        RearLightingServiceEventListenerInterface* event_listener) noexcept = 0;

    /** Returns true when the rear-lighting vsomeip service is currently offered. */
    virtual bool IsServiceAvailable() const noexcept = 0;
};

}  // namespace service
}  // namespace lighting
}  // namespace body_control

#endif  // BODY_CONTROL_LIGHTING_SERVICE_REAR_LIGHTING_SERVICE_INTERFACE_HPP
