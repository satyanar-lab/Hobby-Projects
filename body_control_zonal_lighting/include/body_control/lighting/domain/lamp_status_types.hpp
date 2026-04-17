#ifndef BODY_CONTROL_LIGHTING_DOMAIN_LAMP_STATUS_TYPES_HPP_
#define BODY_CONTROL_LIGHTING_DOMAIN_LAMP_STATUS_TYPES_HPP_

#include <cstdint>

#include "body_control/lighting/domain/lamp_command_types.hpp"

namespace body_control::lighting::domain
{

/**
 * @brief Reported electrical state of a single lamp output.
 *
 * kUnknown must remain at 0 so a zero-initialised struct is never
 * interpreted as a valid "on" or "off" report.
 */
enum class LampOutputState : std::uint8_t
{
    kUnknown = 0U,
    kOff = 1U,
    kOn = 2U
};

/**
 * @brief Status report for a single lamp function.
 *
 * last_sequence_counter mirrors the counter of the most recent command
 * that the node actually applied, so the controller side can correlate
 * request to outcome.
 */
struct LampStatus
{
    LampFunction function {LampFunction::kUnknown};
    LampOutputState output_state {LampOutputState::kUnknown};
    bool command_applied {false};
    std::uint16_t last_sequence_counter {0U};
};

/**
 * @brief Overall operational state of the rear lighting node.
 *
 * kUnavailable is used when the controller-side health monitor detects a
 * heartbeat timeout or when the transport layer reports the service
 * as unreachable. It is distinct from kFaulted, which indicates the node
 * itself reached us but reported a fault.
 */
enum class NodeHealthState : std::uint8_t
{
    kUnknown = 0U,
    kOperational = 1U,
    kDegraded = 2U,
    kFaulted = 3U,
    kUnavailable = 4U
};

/**
 * @brief Aggregated node health snapshot published by the rear node.
 *
 * Field names match the on-wire encoding in lighting_payload_codec.hpp.
 */
struct NodeHealthStatus
{
    NodeHealthState health_state {NodeHealthState::kUnknown};
    bool ethernet_link_available {false};
    bool service_available {false};
    bool lamp_driver_fault_present {false};
    std::uint16_t active_fault_count {0U};
};

/**
 * @brief Returns true if every enum field of the status is within its defined range.
 */
[[nodiscard]] bool IsValidLampStatus(const LampStatus& lamp_status) noexcept;

/**
 * @brief Returns true if every enum field of the node health snapshot is within range.
 */
[[nodiscard]] bool IsValidNodeHealthStatus(
    const NodeHealthStatus& node_health_status) noexcept;

}  // namespace body_control::lighting::domain

#endif  // BODY_CONTROL_LIGHTING_DOMAIN_LAMP_STATUS_TYPES_HPP_
