#ifndef BODY_CONTROL_LIGHTING_DOMAIN_LAMP_STATUS_TYPES_HPP_
#define BODY_CONTROL_LIGHTING_DOMAIN_LAMP_STATUS_TYPES_HPP_

#include <cstdint>

#include "body_control/lighting/domain/lamp_command_types.hpp"

namespace body_control::lighting::domain
{

/**
 * @brief Runtime output state of a lamp function.
 */
enum class LampOutputState : std::uint8_t
{
    kOff      = 0U,
    kOn       = 1U,
    kBlinking = 2U,
    kFault    = 3U,
    kUnknown  = 255U
};

/**
 * @brief Overall health state of the lighting node.
 */
enum class NodeHealthState : std::uint8_t
{
    kInitializing = 0U,
    kOperational  = 1U,
    kDegraded     = 2U,
    kFaulted      = 3U,
    kUnavailable  = 4U,
    kUnknown      = 255U
};

/**
 * @brief Per-lamp runtime status snapshot.
 */
struct LampStatus
{
    LampFunction function {LampFunction::kUnknown};
    LampOutputState output_state {LampOutputState::kUnknown};
    bool command_applied {false};
    std::uint16_t last_sequence_counter {0U};
};

/**
 * @brief Node-wide health snapshot.
 */
struct NodeHealthStatus
{
    NodeHealthState health_state {NodeHealthState::kUnknown};
    bool ethernet_link_available {false};
    bool service_available {false};
    bool lamp_driver_fault_present {false};
    std::uint16_t active_fault_count {0U};
};

constexpr bool IsValidLampOutputState(const LampOutputState state) noexcept
{
    return (state == LampOutputState::kOff)      ||
           (state == LampOutputState::kOn)       ||
           (state == LampOutputState::kBlinking) ||
           (state == LampOutputState::kFault);
}

constexpr bool IsValidNodeHealthState(const NodeHealthState state) noexcept
{
    return (state == NodeHealthState::kInitializing) ||
           (state == NodeHealthState::kOperational)  ||
           (state == NodeHealthState::kDegraded)     ||
           (state == NodeHealthState::kFaulted)      ||
           (state == NodeHealthState::kUnavailable);
}

constexpr bool IsOperationalNode(const NodeHealthStatus& status) noexcept
{
    return (status.health_state == NodeHealthState::kOperational) &&
           status.ethernet_link_available &&
           status.service_available &&
           (!status.lamp_driver_fault_present);
}

constexpr bool IsValidLampStatus(const LampStatus& status) noexcept
{
    return IsValidLampFunction(status.function) &&
           IsValidLampOutputState(status.output_state);
}

constexpr bool IsValidNodeHealthStatus(const NodeHealthStatus& status) noexcept
{
    return IsValidNodeHealthState(status.health_state);
}

}  // namespace body_control::lighting::domain

#endif  // BODY_CONTROL_LIGHTING_DOMAIN_LAMP_STATUS_TYPES_HPP_