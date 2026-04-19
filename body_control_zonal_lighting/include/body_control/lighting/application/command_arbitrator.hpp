#ifndef BODY_CONTROL_LIGHTING_APPLICATION_COMMAND_ARBITRATOR_HPP_
#define BODY_CONTROL_LIGHTING_APPLICATION_COMMAND_ARBITRATOR_HPP_

#include <array>
#include <cstdint>

#include "body_control/lighting/domain/lamp_command_types.hpp"

namespace body_control::lighting::application
{

/**
 * @brief Current command-relevant lighting state used for arbitration.
 */
struct ArbitrationContext
{
    bool left_indicator_active {false};
    bool right_indicator_active {false};
    bool hazard_lamp_active {false};
    bool park_lamp_active {false};
    bool head_lamp_active {false};
};

/**
 * @brief Result classification of arbitration.
 */
enum class ArbitrationResult : std::uint8_t
{
    kAccepted = 0U,
    kRejected = 1U,
    kModified = 2U
};

/**
 * @brief Maximum number of output commands a single arbitration can produce.
 *
 * Hazard expansion produces three commands (hazard + left + right), which
 * is the largest possible output set.
 */
static constexpr std::uint8_t kMaxArbitrationCommands {3U};

/**
 * @brief Final arbitration output.
 *
 * commands[0..command_count-1] holds the ordered sequence of LampCommands
 * to be applied. Fixed-size array avoids heap allocation so Arbitrate()
 * can remain noexcept.
 */
struct ArbitrationDecision
{
    ArbitrationResult result {ArbitrationResult::kRejected};
    std::array<domain::LampCommand, kMaxArbitrationCommands> commands {};
    std::uint8_t command_count {0U};
};

/**
 * @brief Applies command priority and conflict rules.
 *
 * Policy:
 * - Hazard kActivate/kDeactivate expands to three commands:
 *   the hazard command itself plus matching left and right indicator commands.
 * - While hazard is active, indicator activation requests are rejected.
 * - Activating one indicator while the opposite is active automatically
 *   deactivates the opposite (result = kModified, two commands).
 * - Park lamp and head lamp commands are independent of hazard state.
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

    [[nodiscard]] ArbitrationDecision Arbitrate(
        const domain::LampCommand& requested_command,
        const ArbitrationContext& context) const noexcept;

private:
    [[nodiscard]] bool IsHazardCommand(
        const domain::LampCommand& command) const noexcept;

    [[nodiscard]] bool IsIndicatorActivationCommand(
        const domain::LampCommand& command) const noexcept;

    [[nodiscard]] domain::LampCommandAction ResolveHazardAction(
        const domain::LampCommand& command,
        const ArbitrationContext& context) const noexcept;
};

}  // namespace body_control::lighting::application

#endif  // BODY_CONTROL_LIGHTING_APPLICATION_COMMAND_ARBITRATOR_HPP_