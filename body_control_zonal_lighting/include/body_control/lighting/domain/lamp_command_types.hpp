#ifndef BODY_CONTROL_LIGHTING_DOMAIN_LAMP_COMMAND_TYPES_HPP_
#define BODY_CONTROL_LIGHTING_DOMAIN_LAMP_COMMAND_TYPES_HPP_

#include <cstdint>

namespace body_control::lighting::domain
{

/**
 * @brief Identifier for each supported body-control lighting function.
 *
 * Value 0 is reserved as kUnknown so a zeroed message is never mistaken
 * for a valid function request.
 */
enum class LampFunction : std::uint8_t
{
    kUnknown = 0U,
    kLeftIndicator = 1U,
    kRightIndicator = 2U,
    kHazardLamp = 3U,
    kParkLamp = 4U,
    kHeadLamp = 5U
};

/**
 * @brief Action that the requester wants applied to a lamp function.
 */
enum class LampCommandAction : std::uint8_t
{
    kNoAction = 0U,
    kActivate = 1U,
    kDeactivate = 2U,
    kToggle = 3U
};

/**
 * @brief Origin of a lamp command, used for arbitration and logging.
 */
enum class CommandSource : std::uint8_t
{
    kUnknown = 0U,
    kHmiControlPanel = 1U,
    kDiagnosticConsole = 2U,
    kCentralZoneController = 3U
};

/**
 * @brief A single lamp command flowing from requester to controller to node.
 *
 * The sequence_counter is a monotonically increasing 16-bit value that
 * wraps at its maximum. It is sized to match the on-wire payload layout
 * defined in lighting_payload_codec.hpp (2 bytes).
 */
struct LampCommand
{
    LampFunction function {LampFunction::kUnknown};
    LampCommandAction action {LampCommandAction::kNoAction};
    CommandSource source {CommandSource::kUnknown};
    std::uint16_t sequence_counter {0U};
};

/**
 * @brief Returns true if every enum field of the command is within its defined range.
 *
 * Structural validation only: a kNoAction command is valid structurally
 * even though arbitration may still reject it.
 */
[[nodiscard]] bool IsValidLampCommand(const LampCommand& lamp_command) noexcept;

}  // namespace body_control::lighting::domain

#endif  // BODY_CONTROL_LIGHTING_DOMAIN_LAMP_COMMAND_TYPES_HPP_
