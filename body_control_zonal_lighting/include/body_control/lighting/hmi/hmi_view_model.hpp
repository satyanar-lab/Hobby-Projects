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

class HmiViewModel final
{
public:
    HmiViewModel() noexcept;

    void UpdateLampStatus(
        const domain::LampStatus& lamp_status) noexcept;

    void UpdateNodeHealthStatus(
        const domain::NodeHealthStatus& node_health_status) noexcept;

    bool GetLampStatus(
        domain::LampFunction lamp_function,
        domain::LampStatus& lamp_status) const noexcept;

    bool IsLampFunctionActive(
        domain::LampFunction lamp_function) const noexcept;

    domain::NodeHealthStatus GetNodeHealthStatus() const noexcept;

private:
    static std::size_t LampFunctionToIndex(
        domain::LampFunction lamp_function) noexcept;

    std::array<domain::LampStatus, 5U> lamp_statuses_;
    domain::NodeHealthStatus node_health_status_;
};

}  // namespace hmi
}  // namespace lighting
}  // namespace body_control

#endif  // BODY_CONTROL_LIGHTING_HMI_HMI_VIEW_MODEL_HPP