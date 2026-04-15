#ifndef BODY_CONTROL_LIGHTING_HMI_HMI_COMMAND_MAPPER_HPP
#define BODY_CONTROL_LIGHTING_HMI_HMI_COMMAND_MAPPER_HPP

#include "body_control/lighting/hmi/hmi_action_types.hpp"

namespace body_control
{
namespace lighting
{
namespace hmi
{

class HmiCommandMapper final
{
public:
    static HmiAction MapInputToAction(
        char input_key) noexcept;
};

}  // namespace hmi
}  // namespace lighting
}  // namespace body_control

#endif  // BODY_CONTROL_LIGHTING_HMI_HMI_COMMAND_MAPPER_HPP