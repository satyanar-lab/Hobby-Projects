#ifndef BODY_CONTROL_LIGHTING_APPLICATION_REAR_LIGHTING_FUNCTION_MANAGER_HPP
#define BODY_CONTROL_LIGHTING_APPLICATION_REAR_LIGHTING_FUNCTION_MANAGER_HPP

#include <array>
#include <cstddef>

#include "body_control/lighting/domain/lamp_command_types.hpp"
#include "body_control/lighting/domain/lamp_status_types.hpp"

namespace body_control
{
namespace lighting
{
namespace application
{

/**
 * Manages the output state of all rear lamp functions on the node side.
 *
 * This class runs inside the rear lighting node (physical STM32 or Linux
 * simulator).  It receives LampCommands from the service provider, updates
 * the logical on/off state of the targeted function, and serves the current
 * LampStatus back to the service layer for event publishing.
 *
 * The actual GPIO toggling is performed by the platform GPIO driver; this
 * class tracks logical state only and is platform-independent.
 */
class RearLightingFunctionManager final
{
public:
    RearLightingFunctionManager() noexcept;

    /**
     * Applies a received command to the appropriate lamp function.
     *
     * Translates the LampCommandAction (activate / deactivate / toggle) into
     * a LampOutputState change and stores it so the next GetLampStatus() call
     * reflects the updated state.
     *
     * @param lamp_command  Command to apply; must carry a valid LampFunction.
     * @return false if lamp_command.function is kUnknown or out of range.
     */
    bool ApplyCommand(
        const domain::LampCommand& lamp_command) noexcept;

    /**
     * Reads the current status for the given lamp function.
     *
     * Called by the service provider when building a status event or answering
     * a GetLampStatus request.
     *
     * @param lamp_function  Which lamp to query.
     * @param lamp_status    Populated with the current state on true.
     * @return false if lamp_function is kUnknown or no status is available yet.
     */
    bool GetLampStatus(
        domain::LampFunction lamp_function,
        domain::LampStatus& lamp_status) const noexcept;

private:
    /** Maps a LampFunction enum value to a zero-based array index (0–4). */
    static std::size_t LampFunctionToIndex(
        domain::LampFunction lamp_function) noexcept;

    /// One entry per lamp function; indexed by LampFunctionToIndex().
    std::array<domain::LampStatus, 5U> lamp_statuses_;
};

}  // namespace application
}  // namespace lighting
}  // namespace body_control

#endif  // BODY_CONTROL_LIGHTING_APPLICATION_REAR_LIGHTING_FUNCTION_MANAGER_HPP
