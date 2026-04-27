#ifndef BODY_CONTROL_LIGHTING_DOMAIN_LAMP_COMMAND_TYPES_HPP_
#define BODY_CONTROL_LIGHTING_DOMAIN_LAMP_COMMAND_TYPES_HPP_

#include <cstdint>

namespace body_control::lighting::domain
{

/**
 * Identifier for each supported exterior lighting function on the vehicle.
 *
 * Commands and status reports both carry this field so every message is
 * self-describing — no side channel is needed to know which lamp is being
 * referenced.  Value 0 (kUnknown) is reserved so a zero-initialised or
 * corrupted message is never silently treated as a valid lamp request.
 */
enum class LampFunction : std::uint8_t
{
    kUnknown        = 0U,  ///< Sentinel — never a valid target; used for zero-init detection.
    kLeftIndicator  = 1U,  ///< Left turn-signal / direction indicator.
    kRightIndicator = 2U,  ///< Right turn-signal / direction indicator.
    kHazardLamp     = 3U,  ///< Hazard warning — flashes all four indicators simultaneously.
    kParkLamp       = 4U,  ///< Side-marker / parking lamp, low-intensity steady output.
    kHeadLamp       = 5U   ///< Low-beam headlamp.
};

/**
 * The operation the requester wants the controller to apply to a lamp function.
 *
 * kToggle is provided so the HMI can send a single message without needing
 * to know the current state — the controller performs the read-modify-write.
 */
enum class LampCommandAction : std::uint8_t
{
    kNoAction   = 0U,  ///< No change requested; used as a safe default for zero-init.
    kActivate   = 1U,  ///< Turn the lamp on (or begin blinking for indicator/hazard).
    kDeactivate = 2U,  ///< Turn the lamp off unconditionally.
    kToggle     = 3U   ///< Flip current state: on→off or off→on.
};

/**
 * Identifies who issued the command.
 *
 * Recorded in the command and surfaced in logs so operators can distinguish
 * HMI-driven changes from automated or diagnostic ones.  The arbitrator also
 * uses this field to apply source-priority rules when two sources conflict.
 */
enum class CommandSource : std::uint8_t
{
    kUnknown              = 0U,  ///< Source not set; treated as lowest priority.
    kHmiControlPanel      = 1U,  ///< Operator action from the HMI dashboard.
    kDiagnosticConsole    = 2U,  ///< Command issued via the engineering diagnostic tool.
    kCentralZoneController = 3U  ///< Internally generated command by the controller logic.
};

/**
 * A single lamp command as it flows from requester → controller → rear node.
 *
 * The struct is the unit of work throughout the command pipeline: the HMI
 * fills it, the arbitrator validates and prioritises it, and the service
 * layer serialises it onto the wire via lighting_payload_codec.hpp.
 */
struct LampCommand
{
    /** Which lamp this command targets. */
    LampFunction function {LampFunction::kUnknown};

    /** The operation to perform on that lamp. */
    LampCommandAction action {LampCommandAction::kNoAction};

    /** Who issued this command — recorded for arbitration and audit. */
    CommandSource source {CommandSource::kUnknown};

    /**
     * Monotonically increasing per-command counter, wrapping at 65 535.
     * The rear node echoes this value back in its LampStatus so the
     * controller can confirm exactly which command the node last executed.
     */
    std::uint16_t sequence_counter {0U};
};

/**
 * Returns true if every enum field in the command is within its defined range.
 *
 * Structural validation only — a kNoAction command is structurally valid even
 * though the arbitrator may still discard it.  Call before forwarding any
 * command received from an external transport to catch wire-level corruption.
 */
[[nodiscard]] bool IsValidLampCommand(const LampCommand& lamp_command) noexcept;

}  // namespace body_control::lighting::domain

#endif  // BODY_CONTROL_LIGHTING_DOMAIN_LAMP_COMMAND_TYPES_HPP_
