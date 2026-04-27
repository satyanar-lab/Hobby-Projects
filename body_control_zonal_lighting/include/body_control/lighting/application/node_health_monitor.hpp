#ifndef BODY_CONTROL_LIGHTING_APPLICATION_NODE_HEALTH_MONITOR_HPP_
#define BODY_CONTROL_LIGHTING_APPLICATION_NODE_HEALTH_MONITOR_HPP_

#include <chrono>

#include "body_control/lighting/domain/lamp_status_types.hpp"

namespace body_control::lighting::application
{

/**
 * Tracks the operational health of the rear lighting node and detects
 * heartbeat timeouts.
 *
 * The rear node publishes a NodeHealthStatus at kNodeHealthPublishPeriod
 * (1 s).  Each main-loop tick calls ProcessMainLoop() with the elapsed time;
 * if kNodeHeartbeatTimeout (2 s) passes without an update, the monitor
 * marks the node as kUnavailable.  When a fresh health report arrives it
 * resets the timer and updates the cached snapshot.
 *
 * Not thread-safe; the controller serialises access with cache_mutex_.
 */
class NodeHealthMonitor
{
public:
    NodeHealthMonitor() noexcept;
    ~NodeHealthMonitor() = default;

    NodeHealthMonitor(const NodeHealthMonitor&) = delete;
    NodeHealthMonitor& operator=(const NodeHealthMonitor&) = delete;
    NodeHealthMonitor(NodeHealthMonitor&&) = delete;
    NodeHealthMonitor& operator=(NodeHealthMonitor&&) = delete;

    /** Clears the cached snapshot and resets the heartbeat timer to zero. */
    void Reset() noexcept;

    /**
     * Stores a new health snapshot and resets the heartbeat watchdog timer.
     *
     * Called by the controller's OnNodeHealthStatusReceived() callback on
     * every event push from the rear node.
     *
     * @param node_health_status  The freshly received snapshot.
     */
    void UpdateNodeHealthStatus(
        const domain::NodeHealthStatus& node_health_status) noexcept;

    /**
     * Records whether the vsomeip service is currently offered on the network.
     *
     * A service-down event should be treated as immediate unavailability
     * regardless of the heartbeat timer, so the controller calls this from
     * OnServiceAvailabilityChanged() to force a state transition.
     *
     * @param is_available  true when the rear-lighting vsomeip service is offered.
     */
    void SetServiceAvailability(bool is_available) noexcept;

    /**
     * Advances the heartbeat watchdog by the given elapsed time.
     *
     * If the accumulated time since the last UpdateNodeHealthStatus() call
     * exceeds kNodeHeartbeatTimeout, the cached health state is downgraded
     * to kUnavailable.  Called once per main-loop tick.
     *
     * @param elapsed_time  Time that has passed since the previous tick.
     */
    void ProcessMainLoop(
        std::chrono::milliseconds elapsed_time) noexcept;

    /**
     * Reads the current cached node health snapshot.
     *
     * @param node_health_status  Populated with the cached value on true.
     * @return false if no health report has been received since the last Reset().
     */
    [[nodiscard]] bool GetNodeHealthStatus(
        domain::NodeHealthStatus& node_health_status) const noexcept;

    /**
     * Returns true when the rear node is reachable and its heartbeat is current.
     *
     * Equivalent to checking health_state != kUnavailable && service is offered.
     */
    [[nodiscard]] bool IsNodeAvailable() const noexcept;

private:
    /// Last health snapshot received from the node; defaults to all-unknown.
    domain::NodeHealthStatus cached_node_health_status_ {};

    /// Accumulates time since the last UpdateNodeHealthStatus() call.
    std::chrono::milliseconds time_since_last_health_update_ {0};

    /// Set true on the first UpdateNodeHealthStatus() call; prevents false
    /// timeout on startup before the first packet arrives.
    bool health_status_received_ {false};
};

}  // namespace body_control::lighting::application

#endif  // BODY_CONTROL_LIGHTING_APPLICATION_NODE_HEALTH_MONITOR_HPP_
