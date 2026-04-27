#ifndef BODY_CONTROL_LIGHTING_PLATFORM_STM32_GPIO_OUTPUT_DRIVER_HPP_
#define BODY_CONTROL_LIGHTING_PLATFORM_STM32_GPIO_OUTPUT_DRIVER_HPP_

#include <cstdint>

#include "body_control/lighting/domain/lamp_command_types.hpp"

namespace body_control::lighting::platform::stm32
{

/** Return status for GPIO driver operations. */
enum class GpioDriverStatus : std::uint8_t
{
    kSuccess = 0U,              ///< Pin write completed without error.
    kNotInitialized = 1U,       ///< Initialize() has not been called.
    kInvalidArgument = 2U,      ///< LampFunction is kUnknown or has no pin mapping.
    kHardwareAccessFailed = 3U  ///< HAL port or pin lookup returned null/zero.
};

/** STM32 HAL GPIO output driver for the five lamp control pins.
 *
 *  Initialize() enables GPIOB clock and configures pins B1–B5 as push-pull
 *  outputs.  WriteLampOutput() maps a LampFunction to a PinAssignment via the
 *  NUCLEO-H753ZI pin map and calls HAL_GPIO_WritePin.
 *
 *  When compiled without USE_HAL_DRIVER (host unit-test builds), HAL calls are
 *  omitted and the driver returns kSuccess so tests can exercise callers without
 *  a hardware target. */
class GpioOutputDriver
{
public:
    GpioOutputDriver() noexcept = default;
    ~GpioOutputDriver() = default;

    GpioOutputDriver(const GpioOutputDriver&) = delete;
    GpioOutputDriver& operator=(const GpioOutputDriver&) = delete;
    GpioOutputDriver(GpioOutputDriver&&) = delete;
    GpioOutputDriver& operator=(GpioOutputDriver&&) = delete;

    /** Enables GPIOB clock and configures lamp output pins as push-pull low-speed outputs. */
    [[nodiscard]] GpioDriverStatus Initialize();

    /** Drives all lamp output pins to GPIO_PIN_RESET (lamps off). */
    [[nodiscard]] GpioDriverStatus ResetAllOutputs();

    /** Sets the GPIO pin for function to active (high) or inactive (low). */
    [[nodiscard]] GpioDriverStatus WriteLampOutput(
        domain::LampFunction function,
        bool is_active);

    [[nodiscard]] bool IsInitialized() const noexcept;

private:
    bool is_initialized_ {false};
};

}  // namespace body_control::lighting::platform::stm32

#endif  // BODY_CONTROL_LIGHTING_PLATFORM_STM32_GPIO_OUTPUT_DRIVER_HPP_