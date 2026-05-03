#ifndef BODY_CONTROL_LIGHTING_DOMAIN_FAULT_TYPES_HPP_
#define BODY_CONTROL_LIGHTING_DOMAIN_FAULT_TYPES_HPP_

#include <array>
#include <cstdint>
#include <string_view>

#include "body_control/lighting/domain/lamp_command_types.hpp"

namespace body_control::lighting::domain
{

/**
 * Diagnostic Trouble Code (DTC) identifying which lamp circuit has a detected fault.
 *
 * Values are chosen in the 0xBxxx range to align with SAE J2012 powertrain and
 * body DTC classification (B = body).  kNoFault = 0 satisfies the kUnknown = 0
 * sentinel rule: a zeroed DTC slot is never silently treated as an active fault.
 */
enum class FaultCode : std::uint16_t
{
    kNoFault        = 0x0000U,  ///< Sentinel — no fault present; used for zero-init detection.
    kLeftIndicator  = 0xB001U,  ///< Left turn-signal driver fault (short/open circuit).
    kRightIndicator = 0xB002U,  ///< Right turn-signal driver fault.
    kHazardLamp     = 0xB003U,  ///< Hazard lamp driver fault.
    kParkLamp       = 0xB004U,  ///< Park lamp driver fault.
    kHeadLamp       = 0xB005U,  ///< Headlamp driver fault.
};

/**
 * Returns the DTC code that corresponds to a given LampFunction.
 *
 * Used when injecting a fault to record the DTC into the active-fault table.
 * kUnknown input returns kNoFault.
 */
[[nodiscard]] FaultCode LampFunctionToFaultCode(
    LampFunction function) noexcept;

/** Returns a short human-readable label for a FaultCode (e.g. "B001:LeftIndicator"). */
[[nodiscard]] std::string_view FaultCodeToString(FaultCode code) noexcept;

/**
 * The operation requested when a fault command is sent to the rear node.
 *
 * Kept separate from LampCommandAction to prevent fault verbs from polluting
 * the command dispatch logic used by HMI and arbitrator code.
 */
enum class FaultAction : std::uint8_t
{
    kUnknown = 0U,  ///< Sentinel — never a valid fault action.
    kInject  = 1U,  ///< Simulate a driver fault on the named function.
    kClear   = 2U,  ///< Remove the simulated driver fault.
};

/**
 * A single fault-injection command as it flows from diagnostic tool → controller → rear node.
 *
 * Encoded by the same 8-byte fixed-payload rule used for LampCommand; see
 * lighting_payload_codec.hpp for the byte-level layout.
 */
struct FaultCommand
{
    /** Which lamp function the fault targets. */
    LampFunction function {LampFunction::kUnknown};

    /** Whether to inject or clear the fault. */
    FaultAction action {FaultAction::kUnknown};

    /** Originator identifier. */
    CommandSource source {CommandSource::kUnknown};

    /**
     * Monotonically increasing counter wrapping at 65 535.
     * Mirrors the sequence_counter field in LampCommand for consistency.
     */
    std::uint16_t sequence_counter {0U};
};

/**
 * Returns true if every enum field in the fault command is within its defined range.
 *
 * Call before forwarding any FaultCommand received from an external transport.
 */
[[nodiscard]] bool IsValidFaultCommand(const FaultCommand& fault_command) noexcept;

/**
 * Snapshot of the rear node's active fault table, returned by FaultManager and
 * sent in response to a GetFaultStatus request.
 *
 * At most kMaxActiveDtcs faults can be active simultaneously (one per LampFunction).
 * Slots beyond active_fault_count are always kNoFault.
 */
struct LampFaultStatus
{
    static constexpr std::size_t kMaxActiveDtcs {5U};

    bool         fault_present    {false};
    std::uint8_t active_fault_count {0U};

    // Indexed 0 … active_fault_count−1; remaining slots are kNoFault.
    std::array<FaultCode, kMaxActiveDtcs> active_faults {};
};

}  // namespace body_control::lighting::domain

#endif  // BODY_CONTROL_LIGHTING_DOMAIN_FAULT_TYPES_HPP_
