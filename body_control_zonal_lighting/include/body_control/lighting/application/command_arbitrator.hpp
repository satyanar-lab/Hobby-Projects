#ifndef BODY_CONTROL_LIGHTING_APPLICATION_COMMAND_ARBITRATOR_HPP_
#define BODY_CONTROL_LIGHTING_APPLICATION_COMMAND_ARBITRATOR_HPP_

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
 * @brief Final arbitration output.
 */
struct ArbitrationDecision
{
    ArbitrationResult result {ArbitrationResult::kRejected};
    domain::LampCommand command {};
};

/**
 * @brief Applies command priority and conflict rules.
 *
 * Current intended policy:
 * - Hazard activation has priority over left/right indicator activation
 * - While hazard is active, left/right indicator activation requests are rejected
 * - Park lamp and head lamp commands remain independent
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
    [[nodiscard]] bool IsHazardActivationCommand(
        const domain::LampCommand& command) const noexcept;

    [[nodiscard]] bool IsIndicatorActivationCommand(
        const domain::LampCommand& command) const noexcept;
};

}  // namespace body_control::lighting::application

#endif  // BODY_CONTROL_LIGHTING_APPLICATION_COMMAND_ARBITRATOR_HPP_