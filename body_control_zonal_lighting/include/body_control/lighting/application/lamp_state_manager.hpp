#ifndef BODY_CONTROL_LIGHTING_APPLICATION_LAMP_STATE_MANAGER_HPP_
#define BODY_CONTROL_LIGHTING_APPLICATION_LAMP_STATE_MANAGER_HPP_

#include <array>
#include <cstddef>
#include <cstdint>

#include "body_control/lighting/application/command_arbitrator.hpp"
#include "body_control/lighting/domain/lamp_status_types.hpp"

namespace body_control::lighting::application
{

/**
 * @brief Stores and serves the latest known lamp status values.
 */
class LampStateManager
{
public:
    LampStateManager() noexcept;
    ~LampStateManager() = default;

    LampStateManager(const LampStateManager&) = delete;
    LampStateManager& operator=(const LampStateManager&) = delete;
    LampStateManager(LampStateManager&&) = delete;
    LampStateManager& operator=(LampStateManager&&) = delete;

    void Reset() noexcept;

    [[nodiscard]] bool UpdateLampStatus(
        const domain::LampStatus& lamp_status) noexcept;

    [[nodiscard]] bool GetLampStatus(
        domain::LampFunction function,
        domain::LampStatus& lamp_status) const noexcept;

    [[nodiscard]] bool IsFunctionActive(
        domain::LampFunction function) const noexcept;

    [[nodiscard]] ArbitrationContext GetArbitrationContext() const noexcept;

private:
    static constexpr std::size_t kManagedLampCount {5U};

    [[nodiscard]] static bool TryGetIndex(
        domain::LampFunction function,
        std::size_t& index) noexcept;

    [[nodiscard]] static bool IsOutputStateActive(
        domain::LampOutputState output_state) noexcept;

    std::array<domain::LampStatus, kManagedLampCount> lamp_status_cache_ {};
};

}  // namespace body_control::lighting::application

#endif  // BODY_CONTROL_LIGHTING_APPLICATION_LAMP_STATE_MANAGER_HPP_