#ifndef BODY_CONTROL_LIGHTING_SERVICE_OPERATOR_SERVICE_INTERFACE_HPP_
#define BODY_CONTROL_LIGHTING_SERVICE_OPERATOR_SERVICE_INTERFACE_HPP_

#include <cstdint>

#include "body_control/lighting/domain/lamp_command_types.hpp"
#include "body_control/lighting/domain/lamp_status_types.hpp"

namespace body_control
{
namespace lighting
{
namespace service
{

/**
 * Return status for operator service operations.
 *
 * kRejected is distinct from kTransportError: the message was delivered to
 * the controller successfully, but the arbitration policy blocked the
 * command (e.g. an indicator activate request arrived while hazard was active).
 */
enum class OperatorServiceStatus : std::uint8_t
{
    kSuccess         = 0U,  ///< Operation completed or request delivered without error.
    kNotInitialized  = 1U,  ///< Initialize() has not yet been called.
    kNotAvailable    = 2U,  ///< Controller is not reachable over the operator transport.
    kInvalidArgument = 3U,  ///< A LampFunction or other parameter is out of its valid range.
    kRejected        = 4U,  ///< Controller received the request but arbitration blocked it.
    kTransportError  = 5U,  ///< UDP send or receive failed at the transport layer.
};

/**
 * Event callbacks pushed to HMI and diagnostic-console clients.
 *
 * The OperatorServiceProvider fires these callbacks whenever the Central Zone
 * Controller's state changes, so thin clients can render the current lamp
 * state without polling.  All callbacks are delivered on the transport
 * receive thread; implementations must not block.
 */
class OperatorServiceEventListenerInterface
{
public:
    virtual ~OperatorServiceEventListenerInterface() = default;

    /** Called when the controller broadcasts a fresh LampStatus for any function. */
    virtual void OnLampStatusUpdated(
        const domain::LampStatus& lamp_status) = 0;

    /** Called when the controller broadcasts a fresh NodeHealthStatus snapshot. */
    virtual void OnNodeHealthUpdated(
        const domain::NodeHealthStatus& node_health_status) = 0;

    /**
     * Called when the operator transport detects that the controller has
     * come online or gone offline.
     *
     * @param is_available  true = controller reachable; false = lost contact.
     */
    virtual void OnControllerAvailabilityChanged(
        bool is_available) = 0;
};

/**
 * Common interface for both the in-process and out-of-process operator service.
 *
 * Two concrete implementations exist:
 *   - OperatorServiceProvider: runs inside the controller process; requests
 *     are dispatched directly to CentralZoneController without a network hop.
 *   - OperatorServiceConsumer: runs in the HMI / diagnostic-console process;
 *     requests are serialised into UDP datagrams and sent to the controller.
 *
 * The MainWindow and diagnostic console hold a pointer to this interface,
 * so they work identically regardless of which implementation is wired in.
 */
class OperatorServiceProviderInterface
{
public:
    virtual ~OperatorServiceProviderInterface() = default;

    /**
     * Initialises the transport and event subscription.
     * Must be called before any Request* method.
     */
    [[nodiscard]] virtual OperatorServiceStatus Initialize() = 0;

    /** Releases all transport resources and deregisters the event listener. */
    [[nodiscard]] virtual OperatorServiceStatus Shutdown() = 0;

    /**
     * Requests a toggle on the named lamp function.
     *
     * The controller resolves the current state and issues an activate or
     * deactivate command accordingly, applying arbitration rules.
     *
     * @param lamp_function  Which lamp to toggle; kUnknown returns kInvalidArgument.
     */
    [[nodiscard]] virtual OperatorServiceStatus RequestLampToggle(
        domain::LampFunction lamp_function) = 0;

    /**
     * Requests unconditional activation of the named lamp function.
     *
     * @param lamp_function  Target lamp; subject to arbitration on the controller side.
     */
    [[nodiscard]] virtual OperatorServiceStatus RequestLampActivate(
        domain::LampFunction lamp_function) = 0;

    /**
     * Requests unconditional deactivation of the named lamp function.
     *
     * @param lamp_function  Target lamp.
     */
    [[nodiscard]] virtual OperatorServiceStatus RequestLampDeactivate(
        domain::LampFunction lamp_function) = 0;

    /**
     * Requests a fresh NodeHealthStatus snapshot from the controller.
     *
     * The result arrives asynchronously via OnNodeHealthUpdated().
     */
    [[nodiscard]] virtual OperatorServiceStatus RequestNodeHealth() = 0;

    /**
     * Requests the controller to inject a simulated driver fault on the named
     * lamp function at the rear node.
     *
     * @param lamp_function  Target lamp; kUnknown returns kInvalidArgument.
     */
    [[nodiscard]] virtual OperatorServiceStatus RequestInjectFault(
        domain::LampFunction lamp_function) = 0;

    /**
     * Requests the controller to clear the simulated driver fault on the
     * named lamp function at the rear node.
     *
     * @param lamp_function  Target lamp; kUnknown returns kInvalidArgument.
     */
    [[nodiscard]] virtual OperatorServiceStatus RequestClearFault(
        domain::LampFunction lamp_function) = 0;

    /**
     * Requests a full LampFaultStatus snapshot from the controller.
     *
     * The rear node publishes an updated NodeHealth event in response, which
     * propagates back to the operator client via OnNodeHealthUpdated().
     */
    [[nodiscard]] virtual OperatorServiceStatus RequestGetFaultStatus() = 0;

    /**
     * Reads the most recent cached LampStatus for the given function.
     *
     * Non-blocking; never queries the controller.
     *
     * @param lamp_function  Which function to look up.
     * @param lamp_status    Populated on true.
     * @return false if no status has been received yet for this function.
     */
    [[nodiscard]] virtual bool GetLampStatus(
        domain::LampFunction lamp_function,
        domain::LampStatus& lamp_status) const noexcept = 0;

    /**
     * Reads the most recent cached NodeHealthStatus.
     *
     * @param node_health_status  Always populated (defaults to kUnknown if no data yet).
     */
    virtual void GetNodeHealthStatus(
        domain::NodeHealthStatus& node_health_status) const noexcept = 0;

    /**
     * Registers the listener that receives asynchronous event callbacks.
     * Pass nullptr to deregister.
     */
    virtual void SetEventListener(
        OperatorServiceEventListenerInterface* listener) noexcept = 0;
};

}  // namespace service
}  // namespace lighting
}  // namespace body_control

#endif  // BODY_CONTROL_LIGHTING_SERVICE_OPERATOR_SERVICE_INTERFACE_HPP_
