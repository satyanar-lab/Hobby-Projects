#ifndef BODY_CONTROL_LIGHTING_DOMAIN_LAMP_STATUS_TYPES_HPP_
#define BODY_CONTROL_LIGHTING_DOMAIN_LAMP_STATUS_TYPES_HPP_

#include <cstdint>

#include "body_control/lighting/domain/lamp_command_types.hpp"

namespace body_control::lighting::domain
{

/**
 * Reported electrical output state of a single lamp circuit.
 *
 * The rear node reads this directly from the GPIO driver and includes it in
 * every status publish, letting the controller verify the physical output
 * matches what was commanded.  kUnknown at 0 keeps zero-init safe.
 */
enum class LampOutputState : std::uint8_t
{
    kUnknown = 0U,  ///< State not yet read or not reported; treat as indeterminate.
    kOff     = 1U,  ///< GPIO output is low; lamp circuit is de-energised.
    kOn      = 2U   ///< GPIO output is high; lamp circuit is energised.
};

/**
 * Status report for a single lamp function, published by the rear lighting node.
 *
 * The rear node sends one LampStatus per active lamp function at
 * kLampStatusPublishPeriod (100 ms).  The controller stores the latest
 * snapshot per function and forwards it to the HMI on every update.
 */
struct LampStatus
{
    /** Which lamp function this report describes. */
    LampFunction function {LampFunction::kUnknown};

    /** Actual electrical state measured at the GPIO output driver. */
    LampOutputState output_state {LampOutputState::kUnknown};

    /**
     * True when the node has applied the most recent command it received.
     * Distinguishes "command received and acted on" from "node is publishing
     * stale state before the first command arrives".
     */
    bool command_applied {false};

    /**
     * Sequence counter copied from the last LampCommand the node executed.
     * The controller compares this against its own sent counter to detect
     * dropped or out-of-order command delivery.
     */
    std::uint16_t last_sequence_counter {0U};
};

/**
 * Overall operational state of the rear lighting node as assessed by the
 * controller's health monitor.
 *
 * This is a controller-side classification, not a raw self-report from the
 * node.  The health monitor derives it from heartbeat timing, fault flags,
 * and service availability.  kUnavailable means the node stopped responding;
 * kFaulted means it responded but reported a hardware problem.
 */
enum class NodeHealthState : std::uint8_t
{
    kUnknown      = 0U,  ///< Health not yet assessed; initial state on startup.
    kOperational  = 1U,  ///< Node is reachable and all lamps are fault-free.
    kDegraded     = 2U,  ///< Node is reachable but at least one fault is active.
    kFaulted      = 3U,  ///< Node reported a driver-level fault on one or more lamps.
    kUnavailable  = 4U   ///< Heartbeat timeout elapsed; node is unreachable.
};

/**
 * Aggregated health snapshot published by the rear lighting node at
 * kNodeHealthPublishPeriod (1 000 ms).
 *
 * All fields are populated by the node itself based on what it can observe
 * locally — Ethernet link state, vsomeip service availability, and the
 * output driver fault register.
 */
struct NodeHealthStatus
{
    /** Controller-side classification derived from the raw fields below. */
    NodeHealthState health_state {NodeHealthState::kUnknown};

    /** True when the node's Ethernet PHY reports link-up. */
    bool ethernet_link_available {false};

    /** True when the rear lighting vsomeip service is offered and reachable. */
    bool service_available {false};

    /** True when the GPIO output driver has detected a short or open circuit. */
    bool lamp_driver_fault_present {false};

    /** Number of distinct faults currently active in the node's fault table. */
    std::uint16_t active_fault_count {0U};
};

/**
 * Returns true if every enum field in the status is within its defined range.
 *
 * Call before storing or forwarding any LampStatus received from the network
 * to guard against wire-level bit errors or mismatched protocol versions.
 */
[[nodiscard]] bool IsValidLampStatus(const LampStatus& lamp_status) noexcept;

/**
 * Returns true if every enum field in the health snapshot is within range.
 */
[[nodiscard]] bool IsValidNodeHealthStatus(
    const NodeHealthStatus& node_health_status) noexcept;

}  // namespace body_control::lighting::domain

#endif  // BODY_CONTROL_LIGHTING_DOMAIN_LAMP_STATUS_TYPES_HPP_
