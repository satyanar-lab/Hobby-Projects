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

class RearLightingFunctionManager final
{
public:
    RearLightingFunctionManager() noexcept;

    bool ApplyCommand(
        const domain::LampCommand& lamp_command) noexcept;

    bool GetLampStatus(
        domain::LampFunction lamp_function,
        domain::LampStatus& lamp_status) const noexcept;

private:
    static std::size_t LampFunctionToIndex(
        domain::LampFunction lamp_function) noexcept;

    std::array<domain::LampStatus, 5U> lamp_statuses_;
};

}  // namespace application
}  // namespace lighting
}  // namespace body_control

#endif  // BODY_CONTROL_LIGHTING_APPLICATION_REAR_LIGHTING_FUNCTION_MANAGER_HPP
