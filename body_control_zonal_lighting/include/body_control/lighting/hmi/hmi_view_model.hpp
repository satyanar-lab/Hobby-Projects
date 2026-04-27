#ifndef BODY_CONTROL_LIGHTING_HMI_HMI_VIEW_MODEL_HPP
#define BODY_CONTROL_LIGHTING_HMI_HMI_VIEW_MODEL_HPP

#include <array>
#include <cstddef>

#include "body_control/lighting/domain/lamp_status_types.hpp"

namespace body_control
{
namespace lighting
{
namespace hmi
{

/** In-process state cache for the HMI view layer.
 *
 *  Mirrors the lamp and health data published by the operator service so the
 *  terminal UI can render the current system state without issuing a request
 *  for every refresh cycle.  Updated via the OnLamp* callbacks in MainWindow
 *  and read synchronously from the rendering thread. */
class HmiViewModel final
{
public:
    /** Pre-seeds all five lamp slots with kUnknown/kOff defaults so every
     *  GetLampStatus() call returns a valid (if stale) entry before the first
     *  event arrives from the controller. */
    HmiViewModel() noexcept;

    /** Overwrites the cached entry for the lamp identified by lamp_status.function. */
    void UpdateLampStatus(
        const domain::LampStatus& lamp_status) noexcept;

    /** Overwrites the entire node health snapshot. */
    void UpdateNodeHealthStatus(
        const domain::NodeHealthStatus& node_health_status) noexcept;

    /** Copies the cached status for lamp_function into lamp_status.
     *  Returns false if lamp_function is kUnknown or out of range. */
    bool GetLampStatus(
        domain::LampFunction lamp_function,
        domain::LampStatus& lamp_status) const noexcept;

    /** Returns true when the lamp's output_state is kOn; false for kOff, kUnknown,
     *  or any unmapped function. */
    bool IsLampFunctionActive(
        domain::LampFunction lamp_function) const noexcept;

    /** Returns the most recently cached node health snapshot. */
    domain::NodeHealthStatus GetNodeHealthStatus() const noexcept;

private:
    /** Maps LampFunction to a zero-based array index.
     *  Returns SIZE_MAX (−1 cast) for kUnknown or unmapped values so the
     *  caller's bounds check rejects it without a separate flag. */
    static std::size_t LampFunctionToIndex(
        domain::LampFunction lamp_function) noexcept;

    std::array<domain::LampStatus, 5U> lamp_statuses_; ///< One slot per LampFunction (indices 0–4).
    domain::NodeHealthStatus node_health_status_;       ///< Latest health snapshot from the controller.
};

}  // namespace hmi
}  // namespace lighting
}  // namespace body_control

#endif  // BODY_CONTROL_LIGHTING_HMI_HMI_VIEW_MODEL_HPP