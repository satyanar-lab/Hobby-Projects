#include "body_control/lighting/platform/stm32/gpio_output_driver.hpp"

namespace body_control::lighting::platform::stm32
{

GpioDriverStatus GpioOutputDriver::Initialize()
{
    is_initialized_ = true;
    return GpioDriverStatus::kSuccess;
}

GpioDriverStatus GpioOutputDriver::ResetAllOutputs()
{
    if (!is_initialized_)
    {
        return GpioDriverStatus::kNotInitialized;
    }

    return GpioDriverStatus::kSuccess;
}

GpioDriverStatus GpioOutputDriver::WriteLampOutput(
    const domain::LampFunction function,
    const bool is_active)
{
    static_cast<void>(is_active);

    if (!is_initialized_)
    {
        return GpioDriverStatus::kNotInitialized;
    }

    if (!domain::IsValidLampFunction(function))
    {
        return GpioDriverStatus::kInvalidArgument;
    }

    return GpioDriverStatus::kSuccess;
}

bool GpioOutputDriver::IsInitialized() const noexcept
{
    return is_initialized_;
}

}  // namespace body_control::lighting::platform::stm32