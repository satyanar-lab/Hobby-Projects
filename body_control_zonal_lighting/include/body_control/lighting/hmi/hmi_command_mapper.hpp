#ifndef BODY_CONTROL_LIGHTING_HMI_HMI_COMMAND_MAPPER_HPP
#define BODY_CONTROL_LIGHTING_HMI_HMI_COMMAND_MAPPER_HPP

#include "body_control/lighting/hmi/hmi_action_types.hpp"

namespace body_control
{
namespace lighting
{
namespace hmi
{

/** Translates a raw keyboard character into a typed HmiAction.
 *  Stateless; all methods are static so no instance is needed. */
class HmiCommandMapper final
{
public:
    /** Maps a single ASCII key press to the corresponding lamp action.
     *  Returns kUnknown for any key not in the defined key-to-action table. */
    static HmiAction MapInputToAction(
        char input_key) noexcept;
};

}  // namespace hmi
}  // namespace lighting
}  // namespace body_control

#endif  // BODY_CONTROL_LIGHTING_HMI_HMI_COMMAND_MAPPER_HPP