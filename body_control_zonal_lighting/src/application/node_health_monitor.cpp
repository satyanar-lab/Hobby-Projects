#include "body_control/lighting/application/node_health_monitor.hpp"

#include "body_control/lighting/domain/lighting_constants.hpp"

namespace body_control::lighting::application
{

NodeHealthMonitor::NodeHealthMonitor() noexcept
{
    Reset();
}

void NodeHealthMonitor::Reset() noexcept
{
    cached_node_health_status_ = {};
    cached_node_health_status_.health_state = domain::NodeHealthState::kUnknown;
    cached_node_health_status_.ethernet_link_available = false;
    cached_node_health_status_.service_available = false;
    cached_node_health_status_.lamp_driver_fault_present = false;
    cached_node_health_status_.active_fault_count = 0U;

    time_since_last_health_update_ = std::chrono::milliseconds {0};
    health_status_received_ = false;
}

void NodeHealthMonitor::UpdateNodeHealthStatus(
    const domain::NodeHealthStatus& node_health_status) noexcept
{
    if (!domain::IsValidNodeHealthStatus(node_health_status))
    {
        return;
    }

    cached_node_health_status_ = node_health_status;
    // Reset the watchdog timer so ProcessMainLoop() starts counting from now.
    time_since_last_health_update_ = std::chrono::milliseconds {0};
    health_status_received_ = true;
}

void NodeHealthMonitor::SetServiceAvailability(const bool is_available) noexcept
{
    cached_node_health_status_.service_available = is_available;

    // Only escalate to kUnavailable if we have previously seen the node.
    // On startup, before the first packet arrives, health_status_received_ is
    // false and the state is already kUnknown — forcing kUnavailable there
    // would produce a misleading state transition in the HMI.
    if ((!is_available) && health_status_received_)
    {
        cached_node_health_status_.health_state = domain::NodeHealthState::kUnavailable;
    }
}

void NodeHealthMonitor::ProcessMainLoop(
    const std::chrono::milliseconds elapsed_time) noexcept
{
    // Do not start the watchdog until the first packet has been received.
    // This prevents a false timeout on slow network bring-up.
    if (!health_status_received_)
    {
        return;
    }

    time_since_last_health_update_ += elapsed_time;

    if (time_since_last_health_update_ >= domain::timing::kNodeHeartbeatTimeout)
    {
        // Clear link and service flags too — if the heartbeat has timed out,
        // any previously cached link state is stale and must not be trusted.
        cached_node_health_status_.health_state = domain::NodeHealthState::kUnavailable;
        cached_node_health_status_.service_available = false;
        cached_node_health_status_.ethernet_link_available = false;
    }
}

bool NodeHealthMonitor::GetNodeHealthStatus(
    domain::NodeHealthStatus& node_health_status) const noexcept
{
    node_health_status = cached_node_health_status_;
    // Return false if no report has been received — callers can treat a false
    // return as "data not yet available" rather than "node is unhealthy".
    return health_status_received_;
}

bool NodeHealthMonitor::IsNodeAvailable() const noexcept
{
    // All three conditions must hold: we must have seen the node (received at
    // least one packet), the node must not have timed out, and both the vsomeip
    // service and the Ethernet link must be up according to the last report.
    return health_status_received_ &&
           (cached_node_health_status_.health_state != domain::NodeHealthState::kUnavailable) &&
           cached_node_health_status_.service_available &&
           cached_node_health_status_.ethernet_link_available;
}

}  // namespace body_control::lighting::application
