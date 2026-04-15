#include "body_control/lighting/application/rear_lighting_function_manager.hpp"

#include "body_control/lighting/domain/lighting_constants.hpp"

namespace body_control::lighting::application
{

RearLightingFunctionManager::RearLightingFunctionManager() noexcept
{
    Reset();
}

void RearLightingFunctionManager::Reset() noexcept
{
    lamp_status_cache_[0] = {domain::LampFunction::kLeftIndicator, domain::LampOutputState::kOff, false, 0U};
    lamp_status_cache_[1] = {domain::LampFunction::kRightIndicator, domain::LampOutputState::kOff, false, 0U};
    lamp_status_cache_[2] = {domain::LampFunction::kHazardLamp, domain::LampOutputState::kOff, false, 0U};
    lamp_status_cache_[3] = {domain::LampFunction::kParkLamp, domain::LampOutputState::kOff, false, 0U};
    lamp_status_cache_[4] = {domain::LampFunction::kHeadLamp, domain::LampOutputState::kOff, false, 0U};

    blink_phase_on_ = true;
    blink_phase_elapsed_ = std::chrono::milliseconds {0};
}

bool RearLightingFunctionManager::ApplyCommand(
    const domain::LampCommand& command) noexcept
{
    std::size_t index {0U};

    if ((!domain::IsValidLampCommand(command)) ||
        (!TryGetIndex(command.function, index)))
    {
        return false;
    }

    lamp_status_cache_[index].command_applied = true;
    lamp_status_cache_[index].last_sequence_counter = command.sequence_counter;

    if (command.action == domain::LampCommandAction::kDeactivate)
    {
        lamp_status_cache_[index].output_state = domain::LampOutputState::kOff;

        if (command.function == domain::LampFunction::kHazardLamp)
        {
            lamp_status_cache_[0].output_state = domain::LampOutputState::kOff;
            lamp_status_cache_[1].output_state = domain::LampOutputState::kOff;
        }

        return true;
    }

    if (domain::IsBlinkingFunction(command.function))
    {
        lamp_status_cache_[index].output_state = domain::LampOutputState::kBlinking;

        if (command.function == domain::LampFunction::kHazardLamp)
        {
            lamp_status_cache_[0].output_state = domain::LampOutputState::kBlinking;
            lamp_status_cache_[1].output_state = domain::LampOutputState::kBlinking;
        }
    }
    else
    {
        lamp_status_cache_[index].output_state = domain::LampOutputState::kOn;
    }

    return true;
}

void RearLightingFunctionManager::ProcessMainLoop(
    const std::chrono::milliseconds elapsed_time) noexcept
{
    const bool blinking_active =
        IsHazardActive() ||
        IsLeftIndicatorActive() ||
        IsRightIndicatorActive();

    if (!blinking_active)
    {
        blink_phase_on_ = true;
        blink_phase_elapsed_ = std::chrono::milliseconds {0};
        UpdateSteadyOutputs();
        return;
    }

    blink_phase_elapsed_ += elapsed_time;

    const std::chrono::milliseconds current_phase_duration =
        blink_phase_on_ ? domain::timing::kIndicatorOnTime
                        : domain::timing::kIndicatorOffTime;

    if (blink_phase_elapsed_ >= current_phase_duration)
    {
        blink_phase_on_ = !blink_phase_on_;
        blink_phase_elapsed_ = std::chrono::milliseconds {0};
    }

    UpdateBlinkingOutputs();
    UpdateSteadyOutputs();
}

bool RearLightingFunctionManager::GetLampStatus(
    const domain::LampFunction function,
    domain::LampStatus& lamp_status) const noexcept
{
    std::size_t index {0U};

    if (!TryGetIndex(function, index))
    {
        return false;
    }

    lamp_status = lamp_status_cache_[index];
    return true;
}

bool RearLightingFunctionManager::TryGetIndex(
    const domain::LampFunction function,
    std::size_t& index) noexcept
{
    bool is_valid {true};

    switch (function)
    {
    case domain::LampFunction::kLeftIndicator:
        index = 0U;
        break;

    case domain::LampFunction::kRightIndicator:
        index = 1U;
        break;

    case domain::LampFunction::kHazardLamp:
        index = 2U;
        break;

    case domain::LampFunction::kParkLamp:
        index = 3U;
        break;

    case domain::LampFunction::kHeadLamp:
        index = 4U;
        break;

    default:
        is_valid = false;
        break;
    }

    return is_valid;
}

void RearLightingFunctionManager::UpdateBlinkingOutputs() noexcept
{
    if (IsHazardActive())
    {
        lamp_status_cache_[0].output_state = domain::LampOutputState::kBlinking;
        lamp_status_cache_[1].output_state = domain::LampOutputState::kBlinking;
        lamp_status_cache_[2].output_state = domain::LampOutputState::kBlinking;
        return;
    }

    if (IsLeftIndicatorActive())
    {
        lamp_status_cache_[0].output_state = domain::LampOutputState::kBlinking;
    }

    if (IsRightIndicatorActive())
    {
        lamp_status_cache_[1].output_state = domain::LampOutputState::kBlinking;
    }
}

void RearLightingFunctionManager::UpdateSteadyOutputs() noexcept
{
    if (lamp_status_cache_[3].output_state != domain::LampOutputState::kOff)
    {
        lamp_status_cache_[3].output_state = domain::LampOutputState::kOn;
    }

    if (lamp_status_cache_[4].output_state != domain::LampOutputState::kOff)
    {
        lamp_status_cache_[4].output_state = domain::LampOutputState::kOn;
    }
}

bool RearLightingFunctionManager::IsHazardActive() const noexcept
{
    return (lamp_status_cache_[2].output_state == domain::LampOutputState::kBlinking) ||
           (lamp_status_cache_[2].output_state == domain::LampOutputState::kOn);
}

bool RearLightingFunctionManager::IsLeftIndicatorActive() const noexcept
{
    return (lamp_status_cache_[0].output_state == domain::LampOutputState::kBlinking) ||
           (lamp_status_cache_[0].output_state == domain::LampOutputState::kOn);
}

bool RearLightingFunctionManager::IsRightIndicatorActive() const noexcept
{
    return (lamp_status_cache_[1].output_state == domain::LampOutputState::kBlinking) ||
           (lamp_status_cache_[1].output_state == domain::LampOutputState::kOn);
}

}  // namespace body_control::lighting::application