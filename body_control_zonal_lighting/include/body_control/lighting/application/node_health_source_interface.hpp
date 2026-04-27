#ifndef BODY_CONTROL_LIGHTING_APPLICATION_NODE_HEALTH_SOURCE_INTERFACE_HPP_
#define BODY_CONTROL_LIGHTING_APPLICATION_NODE_HEALTH_SOURCE_INTERFACE_HPP_

#include "body_control/lighting/domain/lamp_status_types.hpp"

namespace body_control::lighting::application
{

/**
 * Abstract source of node-side health information for the rear-lighting service.
 *
 * The RearLightingServiceProvider queries an implementation of this interface
 * whenever it builds a GetNodeHealth response or fires a node-health event.
 * Depending on the runtime context, the concrete implementation is either
 * the live NodeHealthMonitor (on the controller) or a stub that generates
 * synthetic data (on the simulator).
 *
 * Keeping the service provider dependent only on this abstract interface
 * prevents the service layer from importing application-layer details.
 */
class NodeHealthSourceInterface
{
public:
    NodeHealthSourceInterface() = default;
    virtual ~NodeHealthSourceInterface() = default;

    NodeHealthSourceInterface(const NodeHealthSourceInterface&) = delete;
    NodeHealthSourceInterface& operator=(
        const NodeHealthSourceInterface&) = delete;
    NodeHealthSourceInterface(NodeHealthSourceInterface&&) = delete;
    NodeHealthSourceInterface& operator=(
        NodeHealthSourceInterface&&) = delete;

    /**
     * Returns the current node health snapshot.
     *
     * Implementations must always return a structurally valid snapshot.
     * If no real data has been observed yet, returning a default-initialised
     * snapshot (kUnknown / false / false / false / 0) is acceptable — callers
     * treat kUnknown health state as "not yet assessed".
     */
    [[nodiscard]] virtual domain::NodeHealthStatus GetNodeHealthSnapshot()
        const noexcept = 0;
};

}  // namespace body_control::lighting::application

#endif  // BODY_CONTROL_LIGHTING_APPLICATION_NODE_HEALTH_SOURCE_INTERFACE_HPP_
