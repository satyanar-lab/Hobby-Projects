#ifndef BODY_CONTROL_LIGHTING_HMI_HMI_COMMAND_MAPPER_HPP_
#define BODY_CONTROL_LIGHTING_HMI_HMI_COMMAND_MAPPER_HPP_

#include <cstdint>

#include "body_control/lighting/domain/lamp_command_types.hpp"

namespace body_control::lighting::hmi
{

/**
 * @brief Supported HMI actions from the control panel.
 */
enum class HmiAction : std::uint8_t
{
    kToggleLeftIndicator = 0U,
    kToggleRightIndicator = 1U,
    kToggleHazardLamp = 2U,
    kToggleParkLamp = 3U,
    kToggleHeadLamp = 4U,
    kRequestNodeHealth = 5U,
    kUnknown = 255U
};

/**
 * @brief HMI-level lamp command request before controller-side sequencing.
 */
struct HmiLampCommandRequest
{
    domain::LampFunction function {domain::LampFunction::kUnknown};
    domain::LampCommandAction action {domain::LampCommandAction::kUnknown};
};

/**
 * @brief Maps HMI interactions into domain command requests.
 */
class HmiCommandMapper
{
public:
    HmiCommandMapper() = default;
    ~HmiCommandMapper() = default;

    HmiCommandMapper(const HmiCommandMapper&) = default;
    HmiCommandMapper& operator=(const HmiCommandMapper&) = default;
    HmiCommandMapper(HmiCommandMapper&&) = default;
    HmiCommandMapper& operator=(HmiCommandMapper&&) = default;

    [[nodiscard]] bool MapActionToLampCommand(
        HmiAction action,
        bool current_active_state,
        HmiLampCommandRequest& command_request) const noexcept;

    [[nodiscard]] bool IsLampControlAction(
        HmiAction action) const noexcept;
};

}  // namespace body_control::lighting::hmi

#endif  // BODY_CONTROL_LIGHTING_HMI_HMI_COMMAND_MAPPER_HPP_