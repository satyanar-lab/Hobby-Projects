#ifndef BODY_CONTROL_LIGHTING_APPLICATION_COMMAND_ARBITRATOR_HPP_
#define BODY_CONTROL_LIGHTING_APPLICATION_COMMAND_ARBITRATOR_HPP_

#include <array>
#include <cstdint>

#include "body_control/lighting/domain/lamp_command_types.hpp"

namespace body_control::lighting::application
{

/**
 * Snapshot of which lamp functions are currently active, passed into
 * Arbitrate() so the arbitrator can apply conflict rules without holding
 * any internal state of its own.
 *
 * The controller builds this from LampStateManager::GetArbitrationContext()
 * immediately before calling Arbitrate(), ensuring decisions are based on
 * the freshest known output state.
 */
struct ArbitrationContext
{
    bool left_indicator_active  {false};  ///< Left indicator is currently flashing.
    bool right_indicator_active {false};  ///< Right indicator is currently flashing.
    bool hazard_lamp_active     {false};  ///< Hazard is active (all four indicators flashing).
    bool park_lamp_active       {false};  ///< Park lamp output is energised.
    bool head_lamp_active       {false};  ///< Headlamp output is energised.
};

/**
 * Classification of the arbitration outcome.
 */
enum class ArbitrationResult : std::uint8_t
{
    kAccepted = 0U,  ///< Command passed through unchanged; forward as-is.
    kRejected = 1U,  ///< Command conflicts with active safety state; discard.
    kModified = 2U,  ///< Command was expanded or adjusted; use the decision's command list.
};

/**
 * Upper bound on commands one arbitration call can emit.
 *
 * Hazard expansion is the worst case: one incoming hazard command produces
 * three outgoing commands (hazard + left indicator + right indicator).
 */
static constexpr std::uint8_t kMaxArbitrationCommands {3U};

/**
 * Output of a single Arbitrate() call.
 *
 * When result is kAccepted or kModified, commands[0..command_count-1]
 * holds the ordered sequence of LampCommands to send to the rear node.
 * The fixed-size array avoids heap allocation so Arbitrate() can be noexcept.
 */
struct ArbitrationDecision
{
    /// Whether the request was accepted, rejected, or accepted with changes.
    ArbitrationResult result {ArbitrationResult::kRejected};

    /// Ordered list of commands to dispatch; valid indices are 0..command_count-1.
    std::array<domain::LampCommand, kMaxArbitrationCommands> commands {};

    /// Number of valid entries in commands[].
    std::uint8_t command_count {0U};
};

/**
 * Stateless arbitrator that enforces automotive lighting conflict rules.
 *
 * Rules applied (in priority order):
 *   1. Hazard kActivate/kDeactivate is expanded to three commands: the hazard
 *      itself plus matching activate/deactivate on both indicators.
 *   2. While hazard is active, any indicator activation request is rejected —
 *      the hazard takes precedence over turn-signalling.
 *   3. Activating one indicator while the opposite is already active
 *      automatically deactivates the opposite (result = kModified, two commands).
 *   4. Park lamp and head lamp commands are independent; no conflict rules apply.
 *
 * The class holds no mutable state so it can be safely called from multiple
 * threads provided the caller serialises access to its own ArbitrationContext.
 */
class CommandArbitrator
{
public:
    CommandArbitrator() = default;
    ~CommandArbitrator() = default;

    CommandArbitrator(const CommandArbitrator&) = default;
    CommandArbitrator& operator=(const CommandArbitrator&) = default;
    CommandArbitrator(CommandArbitrator&&) = default;
    CommandArbitrator& operator=(CommandArbitrator&&) = default;

    /**
     * Evaluate a requested command against the current lighting context and
     * return a decision describing what commands (if any) to send.
     *
     * @param requested_command  The command the caller wants to apply.
     * @param context            Current active/inactive state of all lamp functions.
     * @return ArbitrationDecision — inspect result before reading commands[].
     */
    [[nodiscard]] ArbitrationDecision Arbitrate(
        const domain::LampCommand& requested_command,
        const ArbitrationContext& context) const noexcept;

private:
    /** Returns true if the command targets the hazard lamp function. */
    [[nodiscard]] bool IsHazardCommand(
        const domain::LampCommand& command) const noexcept;

    /** Returns true if the command activates a left or right indicator. */
    [[nodiscard]] bool IsIndicatorActivationCommand(
        const domain::LampCommand& command) const noexcept;

    /**
     * Resolves the effective action for a hazard command when kToggle is used,
     * reading the current hazard state from the context to decide activate vs.
     * deactivate.
     */
    [[nodiscard]] domain::LampCommandAction ResolveHazardAction(
        const domain::LampCommand& command,
        const ArbitrationContext& context) const noexcept;
};

}  // namespace body_control::lighting::application

#endif  // BODY_CONTROL_LIGHTING_APPLICATION_COMMAND_ARBITRATOR_HPP_
