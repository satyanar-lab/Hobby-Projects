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
    time_since_last_health_update_ = std::chrono::milliseconds {0};
    health_status_received_ = true;
}

void NodeHealthMonitor::SetServiceAvailability(const bool is_available) noexcept
{
    cached_node_health_status_.service_available = is_available;

    if ((!is_available) && health_status_received_)
    {
        cached_node_health_status_.health_state = domain::NodeHealthState::kUnavailable;
    }
}

void NodeHealthMonitor::ProcessMainLoop(
    const std::chrono::milliseconds elapsed_time) noexcept
{
    if (!health_status_received_)
    {
        return;
    }

    time_since_last_health_update_ += elapsed_time;

    if (time_since_last_health_update_ >= domain::timing::kNodeHeartbeatTimeout)
    {
        cached_node_health_status_.health_state = domain::NodeHealthState::kUnavailable;
        cached_node_health_status_.service_available = false;
        cached_node_health_status_.ethernet_link_available = false;
    }
}

bool NodeHealthMonitor::GetNodeHealthStatus(
    domain::NodeHealthStatus& node_health_status) const noexcept
{
    node_health_status = cached_node_health_status_;
    return health_status_received_;
}

bool NodeHealthMonitor::IsNodeAvailable() const noexcept
{
    return health_status_received_ &&
           (cached_node_health_status_.health_state != domain::NodeHealthState::kUnavailable) &&
           cached_node_health_status_.service_available &&
           cached_node_health_status_.ethernet_link_available;
}

}  // namespace body_control::lighting::application