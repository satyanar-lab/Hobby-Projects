#ifndef BODY_CONTROL_LIGHTING_APPLICATION_NODE_HEALTH_MONITOR_HPP_
#define BODY_CONTROL_LIGHTING_APPLICATION_NODE_HEALTH_MONITOR_HPP_

#include <chrono>

#include "body_control/lighting/domain/lamp_status_types.hpp"

namespace body_control::lighting::application
{

/**
 * @brief Tracks the latest known node health information and detects timeout.
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

    void Reset() noexcept;

    void UpdateNodeHealthStatus(
        const domain::NodeHealthStatus& node_health_status) noexcept;

    void SetServiceAvailability(bool is_available) noexcept;

    void ProcessMainLoop(
        std::chrono::milliseconds elapsed_time) noexcept;

    [[nodiscard]] bool GetNodeHealthStatus(
        domain::NodeHealthStatus& node_health_status) const noexcept;

    [[nodiscard]] bool IsNodeAvailable() const noexcept;

private:
    domain::NodeHealthStatus cached_node_health_status_ {};
    std::chrono::milliseconds time_since_last_health_update_ {0};
    bool health_status_received_ {false};
};

}  // namespace body_control::lighting::application

#endif  // BODY_CONTROL_LIGHTING_APPLICATION_NODE_HEALTH_MONITOR_HPP_