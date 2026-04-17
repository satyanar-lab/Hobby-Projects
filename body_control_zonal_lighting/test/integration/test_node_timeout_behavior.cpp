/**
 * @file test_node_timeout_behavior.cpp
 * @brief Integration-level check of NodeHealthMonitor timeout behavior.
 *
 * NodeHealthMonitor must:
 *   1. Report the node as unavailable when no health update has ever arrived.
 *   2. Report the node as available once a health status arrives with
 *      ethernet link + service marked available.
 *   3. Transition back to unavailable after kNodeHeartbeatTimeout elapses
 *      without further updates.
 */

#include <chrono>

#include <gtest/gtest.h>

#include "body_control/lighting/application/node_health_monitor.hpp"
#include "body_control/lighting/domain/lighting_constants.hpp"

namespace
{

using body_control::lighting::application::NodeHealthMonitor;
using body_control::lighting::domain::NodeHealthState;
using body_control::lighting::domain::NodeHealthStatus;
using body_control::lighting::domain::timing::kNodeHeartbeatTimeout;

NodeHealthStatus MakeOperationalStatus()
{
    NodeHealthStatus status {};
    status.health_state = NodeHealthState::kOperational;
    status.ethernet_link_available = true;
    status.service_available = true;
    status.lamp_driver_fault_present = false;
    status.active_fault_count = 0U;
    return status;
}

}  // namespace

TEST(NodeTimeoutIntegration, FreshMonitorReportsNodeUnavailable)
{
    const NodeHealthMonitor monitor {};
    EXPECT_FALSE(monitor.IsNodeAvailable());
}

TEST(NodeTimeoutIntegration, OperationalUpdateMarksNodeAvailable)
{
    NodeHealthMonitor monitor {};

    monitor.UpdateNodeHealthStatus(MakeOperationalStatus());

    EXPECT_TRUE(monitor.IsNodeAvailable());
}

TEST(NodeTimeoutIntegration, TimeoutTransitionsToUnavailable)
{
    NodeHealthMonitor monitor {};
    monitor.UpdateNodeHealthStatus(MakeOperationalStatus());
    ASSERT_TRUE(monitor.IsNodeAvailable());

    // Advance just past the heartbeat timeout without any update.
    monitor.ProcessMainLoop(
        kNodeHeartbeatTimeout + std::chrono::milliseconds {1});

    EXPECT_FALSE(monitor.IsNodeAvailable());

    NodeHealthStatus reported {};
    ASSERT_TRUE(monitor.GetNodeHealthStatus(reported));
    EXPECT_EQ(reported.health_state, NodeHealthState::kUnavailable);
    EXPECT_FALSE(reported.service_available);
    EXPECT_FALSE(reported.ethernet_link_available);
}

TEST(NodeTimeoutIntegration, ServiceLossSignalsUnavailable)
{
    NodeHealthMonitor monitor {};
    monitor.UpdateNodeHealthStatus(MakeOperationalStatus());
    ASSERT_TRUE(monitor.IsNodeAvailable());

    monitor.SetServiceAvailability(false);

    EXPECT_FALSE(monitor.IsNodeAvailable());
}
