#ifndef BODY_CONTROL_LIGHTING_APPLICATION_CENTRAL_ZONE_CONTROLLER_HPP
#define BODY_CONTROL_LIGHTING_APPLICATION_CENTRAL_ZONE_CONTROLLER_HPP

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <thread>

#include "body_control/lighting/application/command_arbitrator.hpp"
#include "body_control/lighting/application/lamp_state_manager.hpp"
#include "body_control/lighting/domain/lamp_command_types.hpp"
#include "body_control/lighting/domain/lamp_status_types.hpp"
#include "body_control/lighting/service/rear_lighting_service_interface.hpp"

namespace body_control
{
namespace lighting
{
namespace application
{

/**
 * Return status for all CentralZoneController operations.
 *
 * Every public method returns one of these so the caller never needs to
 * catch an exception to detect failure.
 */
enum class ControllerStatus : std::uint8_t
{
    kSuccess         = 0U,  ///< Operation completed without error.
    kNotInitialized  = 1U,  ///< Initialize() has not been called yet.
    kNotAvailable    = 2U,  ///< The rear lighting node is unreachable.
    kInvalidArgument = 3U,  ///< A parameter (e.g. LampFunction) is out of range.
    kRejected        = 4U,  ///< Arbitration blocked the command (e.g. indicator during hazard).
    kServiceError    = 5U   ///< The underlying transport or vsomeip service reported a failure.
};

/**
 * Central orchestrator for the body-control lighting zone.
 *
 * Sits at the top of the application layer and wires together the command
 * arbitrator, lamp-state cache, health monitor, and rear-lighting service
 * consumer.  The operator-facing service (OperatorServiceProvider) calls
 * into this class to forward HMI and diagnostic-console commands, then
 * receives state updates via the observer registered with SetStatusObserver().
 *
 * Thread safety: public methods are guarded by cache_mutex_ so they may be
 * called from the vsomeip callback thread and the operator-service thread
 * simultaneously.  The health-poll background thread only calls public
 * service-consumer methods and is joined on Shutdown().
 */
class CentralZoneController final
    : public service::RearLightingServiceEventListenerInterface
{
public:
    /**
     * Stores a reference to the rear-lighting service consumer used to send
     * commands and subscribe to events.  Does not start any threads.
     */
    explicit CentralZoneController(
        service::RearLightingServiceConsumerInterface& rear_lighting_service_consumer) noexcept;

    /**
     * Subscribes to the rear-lighting service, resets the state cache, and
     * starts the health-poll background thread.  Must be called once before
     * any SendLampCommand() or Request* call.
     *
     * @return kSuccess on success; kServiceError if subscription fails.
     */
    ControllerStatus Initialize();

    /**
     * Cancels the health-poll thread, unsubscribes from the service, and
     * resets internal state to the pre-initialized condition.
     *
     * @return kSuccess; never fails.
     */
    ControllerStatus Shutdown();

    /**
     * Passes a lamp command through arbitration and, if accepted, serialises
     * and sends it to the rear lighting node.
     *
     * The sequence counter is incremented automatically.  If arbitration
     * expands the command (e.g. hazard → three commands) all expanded
     * commands are sent in order within this call.
     *
     * @param lamp_function      Target lamp.
     * @param lamp_command_action Operation to perform (activate/deactivate/toggle).
     * @param command_source     Originator identifier, recorded for audit.
     * @return kAccepted → kSuccess; kRejected by arbitration → kRejected;
     *         transport failure → kServiceError.
     */
    ControllerStatus SendLampCommand(
        domain::LampFunction lamp_function,
        domain::LampCommandAction lamp_command_action,
        domain::CommandSource command_source);

    /**
     * Sends a GetLampStatus request to the rear node for the given function.
     *
     * The response arrives asynchronously via OnLampStatusReceived().
     *
     * @param lamp_function  Which lamp to query.
     * @return kSuccess or kServiceError.
     */
    ControllerStatus RequestLampStatus(
        domain::LampFunction lamp_function);

    /**
     * Sends a GetNodeHealth request to the rear node.
     *
     * The response arrives asynchronously via OnNodeHealthStatusReceived().
     *
     * @return kSuccess or kServiceError.
     */
    ControllerStatus RequestNodeHealth();

    /**
     * Reads the last-known LampStatus for the given function from the cache.
     *
     * Non-blocking; never queries the network.  Returns false if no status
     * has been received yet for that function (lamp_status is left unchanged).
     *
     * @param lamp_function  Which lamp to look up.
     * @param lamp_status    Populated with the cached value on true.
     */
    bool GetCachedLampStatus(
        domain::LampFunction lamp_function,
        domain::LampStatus& lamp_status) const noexcept;

    /**
     * Returns the last-known aggregated node health snapshot.
     *
     * Returns a default-initialised (kUnknown) snapshot if no health
     * report has been received yet.
     */
    domain::NodeHealthStatus GetCachedNodeHealthStatus() const noexcept;

    /**
     * Returns true if the rear lighting node is currently reachable and
     * its service is offered on the network.
     */
    bool IsRearNodeAvailable() const noexcept;

    /**
     * Register an observer that receives forwarded event callbacks after
     * the controller updates its own cache.
     *
     * Intended for OperatorServiceProvider so it can relay lamp-state and
     * health events to connected HMI clients without polling.  Pass nullptr
     * to deregister.  Only one observer is supported at a time.
     */
    void SetStatusObserver(
        service::RearLightingServiceEventListenerInterface* observer) noexcept;

    /** Callback from the rear-lighting service — updates cache and notifies observer. */
    void OnLampStatusReceived(
        const domain::LampStatus& lamp_status) override;

    /** Callback from the rear-lighting service — updates health cache and notifies observer. */
    void OnNodeHealthStatusReceived(
        const domain::NodeHealthStatus& node_health_status) override;

    /** Callback from vsomeip — marks the rear node as available or unavailable. */
    void OnServiceAvailabilityChanged(
        bool is_service_available) override;

private:
    /** Maps a ServiceStatus from the consumer interface to a ControllerStatus. */
    static ControllerStatus ConvertServiceStatus(
        service::ServiceStatus service_status) noexcept;

    /** Maps a LampFunction enum value to a zero-based array index (0–4). */
    static std::size_t LampFunctionToIndex(
        domain::LampFunction lamp_function) noexcept;

    /**
     * Background thread body: periodically requests a node health snapshot
     * from the rear node while health_poll_active_ is true.  Wakes on a
     * condition variable so Shutdown() can stop it without a sleep delay.
     */
    void RunHealthPollLoop();

    service::RearLightingServiceConsumerInterface& rear_lighting_service_consumer_;

    bool is_initialized_;
    bool is_rear_node_available_;

    /// Monotonically increasing command identifier; wraps at uint16 max.
    std::uint16_t next_sequence_counter_;

    CommandArbitrator arbitrator_;
    LampStateManager lamp_state_manager_;

    /// Protects cached_lamp_statuses_, cached_node_health_status_, and
    /// is_rear_node_available_ against concurrent reads and callback writes.
    mutable std::mutex cache_mutex_;
    std::array<domain::LampStatus, 5U> cached_lamp_statuses_;
    domain::NodeHealthStatus cached_node_health_status_;

    /// Optional observer; notified after every successful cache update.
    service::RearLightingServiceEventListenerInterface* status_observer_;

    std::atomic<bool> health_poll_active_;
    std::mutex        health_poll_mutex_;
    std::condition_variable health_poll_cv_;
    std::thread       health_poll_thread_;
};

}  // namespace application
}  // namespace lighting
}  // namespace body_control

#endif  // BODY_CONTROL_LIGHTING_APPLICATION_CENTRAL_ZONE_CONTROLLER_HPP
