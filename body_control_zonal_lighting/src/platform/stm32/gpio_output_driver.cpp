#include "body_control/lighting/platform/stm32/gpio_output_driver.hpp"
#include "body_control/lighting/platform/stm32/nucleo_h753zi_pin_map.hpp"
#include "body_control/lighting/domain/lamp_command_types.hpp"

#ifdef USE_HAL_DRIVER
#include "stm32h7xx_hal.h"
#endif

namespace body_control::lighting::platform::stm32
{

#ifdef USE_HAL_DRIVER
namespace
{

// Maps the project's logical port ID to the HAL GPIOX peripheral pointer.
GPIO_TypeDef* PortIdToHalPort(const GpioPortId port_id) noexcept
{
    switch (port_id)
    {
        case GpioPortId::kPortA: return GPIOA;
        case GpioPortId::kPortB: return GPIOB;
        case GpioPortId::kPortC: return GPIOC;
        case GpioPortId::kPortD: return GPIOD;
        case GpioPortId::kPortE: return GPIOE;
        case GpioPortId::kPortF: return GPIOF;
        case GpioPortId::kPortG: return GPIOG;
        case GpioPortId::kPortH: return GPIOH;
        case GpioPortId::kPortI: return GPIOI;
        default:                 return nullptr;
    }
}

// HAL uses a bitmask (GPIO_PIN_x = 1 << x) rather than a plain index.
// Returns 0 for any pin number outside the valid 0–15 range so the caller
// can detect an invalid pin without a separate out-of-range check.
std::uint16_t PinNumberToHalPin(const std::uint16_t pin_number) noexcept
{
    if (pin_number > 15U) { return 0U; }
    return static_cast<std::uint16_t>(1U << pin_number);
}

PinAssignment LampFunctionToPinAssignment(
    const domain::LampFunction function) noexcept
{
    using domain::LampFunction;
    switch (function)
    {
        case LampFunction::kLeftIndicator:  return pin_map::kLeftIndicatorOutput;
        case LampFunction::kRightIndicator: return pin_map::kRightIndicatorOutput;
        case LampFunction::kHazardLamp:     return pin_map::kHazardLampOutput;
        case LampFunction::kParkLamp:       return pin_map::kParkLampOutput;
        case LampFunction::kHeadLamp:       return pin_map::kHeadLampOutput;
        default:                             return {};
    }
}

}  // namespace
#endif  // USE_HAL_DRIVER

GpioDriverStatus GpioOutputDriver::Initialize()
{
#ifdef USE_HAL_DRIVER
    // All five lamp pins are on GPIOB; a single clock enable covers them all.
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef gpio_init {};
    gpio_init.Pin  = GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5;
    gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed= GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &gpio_init);

    HAL_GPIO_WritePin(
        GPIOB,
        GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5,
        GPIO_PIN_RESET);
#endif

    is_initialized_ = true;
    return GpioDriverStatus::kSuccess;
}

GpioDriverStatus GpioOutputDriver::ResetAllOutputs()
{
    if (!is_initialized_)
    {
        return GpioDriverStatus::kNotInitialized;
    }

#ifdef USE_HAL_DRIVER
    HAL_GPIO_WritePin(
        GPIOB,
        GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5,
        GPIO_PIN_RESET);
#endif

    return GpioDriverStatus::kSuccess;
}

GpioDriverStatus GpioOutputDriver::WriteLampOutput(
    const domain::LampFunction function,
    const bool is_active)
{
    if (!is_initialized_)
    {
        return GpioDriverStatus::kNotInitialized;
    }

    if (function == domain::LampFunction::kUnknown)
    {
        return GpioDriverStatus::kInvalidArgument;
    }

#ifdef USE_HAL_DRIVER
    const PinAssignment pin = LampFunctionToPinAssignment(function);
    GPIO_TypeDef* const hal_port = PortIdToHalPort(pin.port_id);
    const std::uint16_t hal_pin  = PinNumberToHalPin(pin.pin_number);

    if ((hal_port == nullptr) || (hal_pin == 0U))
    {
        return GpioDriverStatus::kHardwareAccessFailed;
    }

    HAL_GPIO_WritePin(
        hal_port,
        hal_pin,
        is_active ? GPIO_PIN_SET : GPIO_PIN_RESET);
#else
    // Without USE_HAL_DRIVER (host build) there is no GPIO peripheral;
    // suppress the unused-parameter warning and return success so unit tests
    // can exercise callers of this driver without a hardware target.
    static_cast<void>(is_active);
#endif

    return GpioDriverStatus::kSuccess;
}

bool GpioOutputDriver::IsInitialized() const noexcept
{
    return is_initialized_;
}

}  // namespace body_control::lighting::platform::stm32
