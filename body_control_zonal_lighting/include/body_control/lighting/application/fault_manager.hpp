#ifndef BODY_CONTROL_LIGHTING_APPLICATION_FAULT_MANAGER_HPP_
#define BODY_CONTROL_LIGHTING_APPLICATION_FAULT_MANAGER_HPP_

#include <cstdint>

#include "body_control/lighting/domain/fault_types.hpp"
#include "body_control/lighting/domain/lamp_command_types.hpp"
#include "body_control/lighting/domain/lamp_status_types.hpp"

namespace body_control
{
namespace lighting
{
namespace application
{

/**
 * Return status for FaultManager operations.
 *
 * kAlreadyFaulted and kNotFaulted are informational: the call was a no-op
 * but not an error.  Callers may log and ignore them; the service layer maps
 * both to ServiceStatus::kSuccess on the wire.
 */
enum class FaultManagerStatus : std::uint8_t
{
    kSuccess         = 0U,  ///< Fault state changed successfully.
    kInvalidFunction = 1U,  ///< LampFunction is kUnknown or out of range.
    kAlreadyFaulted  = 2U,  ///< InjectFault: function was already in fault state.
    kNotFaulted      = 3U,  ///< ClearFault: function was not faulted — no-op.
};

/**
 * Tracks simulated per-lamp driver faults for the rear lighting node.
 *
 * This class owns the runtime fault table (one bool per LampFunction) and the
 * active DTC array (one FaultCode per slot).  It is owned by
 * RearLightingFunctionManager and has no dependencies on GPIO, transport, or
 * service layers.
 *
 * Threading contract: must be called only from the rear-node control task
 * (the thread or super-loop that processes decoded lamp and fault commands).
 * No synchronisation primitives are used — callers must not invoke these
 * methods from interrupt context or from concurrent threads.
 */
class FaultManager final
{
public:
    FaultManager() noexcept;

    /**
     * Marks the given function as faulted and records its DTC.
     *
     * @return kSuccess on a new fault, kAlreadyFaulted if already in fault
     *         state, kInvalidFunction if function is kUnknown.
     */
    [[nodiscard]] FaultManagerStatus InjectFault(
        domain::LampFunction function) noexcept;

    /**
     * Clears the fault for the given function and removes its DTC.
     *
     * @return kSuccess on a cleared fault, kNotFaulted if not currently
     *         faulted, kInvalidFunction if function is kUnknown.
     */
    [[nodiscard]] FaultManagerStatus ClearFault(
        domain::LampFunction function) noexcept;

    /** Clears all active faults and resets the DTC table. */
    void ClearAllFaults() noexcept;

    /**
     * Returns true when the given function has an active fault.
     *
     * Called by RearLightingFunctionManager::ApplyCommand to block lamp
     * activation while a fault is injected.
     */
    [[nodiscard]] bool IsFaulted(
        domain::LampFunction function) const noexcept;

    /** Returns the count of currently active faults (0–5). */
    [[nodiscard]] std::uint8_t ActiveFaultCount() const noexcept;

    /**
     * Assembles the current fault snapshot for external consumption.
     *
     * Called by RearLightingFunctionManager::GetFaultStatus().
     */
    [[nodiscard]] domain::LampFaultStatus GetFaultStatus() const noexcept;

    /**
     * Writes fault data into an existing NodeHealthStatus snapshot.
     *
     * Sets lamp_driver_fault_present and active_fault_count from the current
     * fault table.  Called by the service provider before publishing health.
     */
    void PopulateHealth(domain::NodeHealthStatus& health) const noexcept;

private:
    /**
     * Maps a LampFunction to a zero-based array index (0–4).
     *
     * Returns SIZE_MAX (≥ array size) for kUnknown or any out-of-range value,
     * letting callers use a single bounds check.
     */
    static std::size_t LampFunctionToIndex(
        domain::LampFunction function) noexcept;

    // One fault flag per lamp function; indexed by LampFunctionToIndex().
    std::array<bool, 5U> faulted_ {};

    // Active DTC codes; slots are compacted (no gaps) and ordered by injection time.
    std::array<domain::FaultCode,
               domain::LampFaultStatus::kMaxActiveDtcs> active_dtcs_ {};

    std::uint8_t active_fault_count_ {0U};
};

}  // namespace application
}  // namespace lighting
}  // namespace body_control

#endif  // BODY_CONTROL_LIGHTING_APPLICATION_FAULT_MANAGER_HPP_
