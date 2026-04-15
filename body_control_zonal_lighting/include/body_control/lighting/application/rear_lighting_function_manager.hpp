#ifndef BODY_CONTROL_LIGHTING_APPLICATION_REAR_LIGHTING_FUNCTION_MANAGER_HPP_
#define BODY_CONTROL_LIGHTING_APPLICATION_REAR_LIGHTING_FUNCTION_MANAGER_HPP_

#include <array>
#include <chrono>
#include <cstddef>

#include "body_control/lighting/domain/lamp_command_types.hpp"
#include "body_control/lighting/domain/lamp_status_types.hpp"

namespace body_control::lighting::application
{

/**
 * @brief Node-side lighting behavior manager.
 *
 * This component applies logical lamp commands and produces runtime lamp
 * statuses. It is intended to be reusable across:
 * - Linux simulator node
 * - STM32 target node
 */
class RearLightingFunctionManager
{
public:
    RearLightingFunctionManager() noexcept;
    ~RearLightingFunctionManager() = default;

    RearLightingFunctionManager(const RearLightingFunctionManager&) = delete;
    RearLightingFunctionManager& operator=(const RearLightingFunctionManager&) = delete;
    RearLightingFunctionManager(RearLightingFunctionManager&&) = delete;
    RearLightingFunctionManager& operator=(RearLightingFunctionManager&&) = delete;

    void Reset() noexcept;

    [[nodiscard]] bool ApplyCommand(
        const domain::LampCommand& command) noexcept;

    void ProcessMainLoop(
        std::chrono::milliseconds elapsed_time) noexcept;

    [[nodiscard]] bool GetLampStatus(
        domain::LampFunction function,
        domain::LampStatus& lamp_status) const noexcept;

private:
    static constexpr std::size_t kManagedLampCount {5U};

    [[nodiscard]] static bool TryGetIndex(
        domain::LampFunction function,
        std::size_t& index) noexcept;

    void UpdateBlinkingOutputs() noexcept;
    void UpdateSteadyOutputs() noexcept;

    [[nodiscard]] bool IsHazardActive() const noexcept;
    [[nodiscard]] bool IsLeftIndicatorActive() const noexcept;
    [[nodiscard]] bool IsRightIndicatorActive() const noexcept;

    std::array<domain::LampStatus, kManagedLampCount> lamp_status_cache_ {};
    bool blink_phase_on_ {true};
    std::chrono::milliseconds blink_phase_elapsed_ {0};
};

}  // namespace body_control::lighting::application

#endif  // BODY_CONTROL_LIGHTING_APPLICATION_REAR_LIGHTING_FUNCTION_MANAGER_HPP_