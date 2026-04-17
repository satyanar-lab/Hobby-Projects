#ifndef BODY_CONTROL_LIGHTING_APPLICATION_NODE_HEALTH_SOURCE_INTERFACE_HPP_
#define BODY_CONTROL_LIGHTING_APPLICATION_NODE_HEALTH_SOURCE_INTERFACE_HPP_

#include "body_control/lighting/domain/lamp_status_types.hpp"

namespace body_control::lighting::application
{

/**
 * @brief Pluggable source of node-side health information.
 *
 * The rear-node service provider queries an implementation of this
 * interface whenever it needs to answer a GetNodeHealth request or
 * build a node health event. Keeping the provider dependent only on
 * this abstract interface keeps the service layer decoupled from the
 * concrete monitor and its internal timing/fault-counting model.
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
     * @brief Returns the current node-health snapshot.
     *
     * Implementations must always return a structurally valid
     * NodeHealthStatus, even if no source data has been observed yet
     * (a kUnknown/false/false/false/0 snapshot is acceptable in that case).
     */
    [[nodiscard]] virtual domain::NodeHealthStatus GetNodeHealthSnapshot()
        const noexcept = 0;
};

}  // namespace body_control::lighting::application

#endif  // BODY_CONTROL_LIGHTING_APPLICATION_NODE_HEALTH_SOURCE_INTERFACE_HPP_
