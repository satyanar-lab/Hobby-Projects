#ifndef BODY_CONTROL_LIGHTING_DOMAIN_LAMP_COMMAND_TYPES_HPP
#define BODY_CONTROL_LIGHTING_DOMAIN_LAMP_COMMAND_TYPES_HPP

#include <cstdint>

namespace body_control
{
namespace lighting
{
namespace domain
{

enum class LampFunction : std::uint8_t
{
    kUnknown = 0U,
    kLeftIndicator = 1U,
    kRightIndicator = 2U,
    kHazardLamp = 3U,
    kParkLamp = 4U,
    kHeadLamp = 5U
};

enum class LampCommandAction : std::uint8_t
{
    kNoAction = 0U,
    kActivate = 1U,
    kDeactivate = 2U,
    kToggle = 3U
};

enum class CommandSource : std::uint8_t
{
    kUnknown = 0U,
    kHmiControlPanel = 1U,
    kDiagnosticConsole = 2U,
    kCentralZoneController = 3U
};

struct LampCommand
{
    LampFunction function {LampFunction::kUnknown};
    LampCommandAction action {LampCommandAction::kNoAction};
    CommandSource source {CommandSource::kUnknown};
    std::uint32_t sequence_counter {0U};
};

}  // namespace domain
}  // namespace lighting
}  // namespace body_control

#endif  // BODY_CONTROL_LIGHTING_DOMAIN_LAMP_COMMAND_TYPES_HPP