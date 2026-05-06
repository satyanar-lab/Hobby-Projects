#ifndef BODY_CONTROL_LIGHTING_VSS_VSS_LAMP_OVERLAY_HPP_
#define BODY_CONTROL_LIGHTING_VSS_VSS_LAMP_OVERLAY_HPP_

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "body_control/lighting/domain/fault_types.hpp"
#include "body_control/lighting/domain/lamp_status_types.hpp"

namespace body_control::lighting::vss {

/**
 * Snapshot of the 11 VSS signal values published by this project.
 *
 * Five standard signals from Vehicle.Body.Lights.* (booleans) and six
 * vendor-extension signals from Vehicle.Private.BCL.Lighting.* (uint16).
 * IsDefect signals are not included: this project does not publish them.
 * Fields are ordered alphabetically by their VSS path, matching ToJson output.
 */
struct VssSnapshot
{
    // Vehicle.Body.Lights.Beam.Low.IsOn
    bool beam_low_is_on                          {false};
    // Vehicle.Body.Lights.DirectionIndicator.Left.IsSignaling
    bool direction_indicator_left_is_signaling   {false};
    // Vehicle.Body.Lights.DirectionIndicator.Right.IsSignaling
    bool direction_indicator_right_is_signaling  {false};
    // Vehicle.Body.Lights.Hazard.IsSignaling
    bool hazard_is_signaling                     {false};
    // Vehicle.Body.Lights.Parking.IsOn
    bool parking_is_on                           {false};

    // Vehicle.Private.BCL.Lighting.CommandSequenceCounter
    std::uint16_t bcl_command_sequence_counter   {0U};
    // Vehicle.Private.BCL.Lighting.Hazard.ActiveFaultCode
    std::uint16_t bcl_hazard_active_fault_code   {0U};
    // Vehicle.Private.BCL.Lighting.HeadLamp.ActiveFaultCode
    std::uint16_t bcl_head_lamp_active_fault_code {0U};
    // Vehicle.Private.BCL.Lighting.LeftIndicator.ActiveFaultCode
    std::uint16_t bcl_left_indicator_active_fault_code  {0U};
    // Vehicle.Private.BCL.Lighting.ParkLamp.ActiveFaultCode
    std::uint16_t bcl_park_lamp_active_fault_code {0U};
    // Vehicle.Private.BCL.Lighting.RightIndicator.ActiveFaultCode
    std::uint16_t bcl_right_indicator_active_fault_code {0U};
};

/**
 * Read-only adapter from BCL domain state to a VSS-shaped snapshot.
 *
 * No transport, no I/O, no global state. Pure computation; safe to call
 * from any thread as long as the caller holds consistent domain snapshots.
 */
class VssLampOverlay
{
public:
    VssLampOverlay() = default;

    /// Number of distinct lamp functions tracked (kLeftIndicator=1 … kHeadLamp=5).
    static constexpr std::size_t kLampFunctionCount {5U};

    /**
     * Build a VSS snapshot from current domain state.
     *
     * @param lamp_statuses  Array of LampStatus, one per function. Each entry
     *   is matched by its .function field, not by position, so the array may
     *   contain entries in any order; entries with function==kUnknown are
     *   ignored. CommandSequenceCounter is taken from the first non-kUnknown
     *   entry.
     * @param fault_status  Current fault table from FaultManager::GetFaultStatus().
     */
    [[nodiscard]] VssSnapshot Snapshot(
        const std::array<domain::LampStatus, kLampFunctionCount>& lamp_statuses,
        const domain::LampFaultStatus& fault_status) const noexcept;

    /**
     * Serialize a snapshot to a flat JSON object.
     *
     * Keys are VSS path strings. Boolean signals serialize as JSON true/false;
     * uint16 signals serialize as JSON decimal integers. Keys are emitted in
     * alphabetical order — deterministic across runs.
     */
    [[nodiscard]] static std::string ToJson(const VssSnapshot& snapshot);
};

}  // namespace body_control::lighting::vss

#endif  // BODY_CONTROL_LIGHTING_VSS_VSS_LAMP_OVERLAY_HPP_
