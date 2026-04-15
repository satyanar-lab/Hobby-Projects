#ifndef BODY_CONTROL_LIGHTING_HMI_HMI_VIEW_MODEL_HPP_
#define BODY_CONTROL_LIGHTING_HMI_HMI_VIEW_MODEL_HPP_

#include <cstdint>

#include "body_control/lighting/domain/lamp_status_types.hpp"

namespace body_control::lighting::hmi
{

/**
 * @brief UI-facing state container for the control panel.
 *
 * This class intentionally stays framework-neutral so it can later be used by
 * different GUI technologies.
 */
class HmiViewModel
{
public:
    HmiViewModel() noexcept = default;
    ~HmiViewModel() = default;

    HmiViewModel(const HmiViewModel&) = default;
    HmiViewModel& operator=(const HmiViewModel&) = default;
    HmiViewModel(HmiViewModel&&) = default;
    HmiViewModel& operator=(HmiViewModel&&) = default;

    void Reset() noexcept;

    void UpdateLampStatus(
        const domain::LampStatus& lamp_status) noexcept;

    void UpdateNodeHealthStatus(
        const domain::NodeHealthStatus& node_health_status) noexcept;

    [[nodiscard]] bool IsLampFunctionActive(
        domain::LampFunction function) const noexcept;

    [[nodiscard]] domain::NodeHealthStatus GetNodeHealthStatus() const noexcept;

private:
    bool left_indicator_active_ {false};
    bool right_indicator_active_ {false};
    bool hazard_lamp_active_ {false};
    bool park_lamp_active_ {false};
    bool head_lamp_active_ {false};

    domain::NodeHealthStatus node_health_status_ {};
};

}  // namespace body_control::lighting::hmi

#endif  // BODY_CONTROL_LIGHTING_HMI_HMI_VIEW_MODEL_HPP_