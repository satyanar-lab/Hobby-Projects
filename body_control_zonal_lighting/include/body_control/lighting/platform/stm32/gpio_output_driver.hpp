#ifndef BODY_CONTROL_LIGHTING_PLATFORM_STM32_GPIO_OUTPUT_DRIVER_HPP_
#define BODY_CONTROL_LIGHTING_PLATFORM_STM32_GPIO_OUTPUT_DRIVER_HPP_

#include <cstdint>

#include "body_control/lighting/domain/lamp_command_types.hpp"

namespace body_control::lighting::platform::stm32
{

/**
 * @brief Result status for GPIO output operations.
 */
enum class GpioDriverStatus : std::uint8_t
{
    kSuccess = 0U,
    kNotInitialized = 1U,
    kInvalidArgument = 2U,
    kHardwareAccessFailed = 3U
};

/**
 * @brief STM32 GPIO output driver for lamp control pins.
 */
class GpioOutputDriver
{
public:
    GpioOutputDriver() noexcept = default;
    ~GpioOutputDriver() = default;

    GpioOutputDriver(const GpioOutputDriver&) = delete;
    GpioOutputDriver& operator=(const GpioOutputDriver&) = delete;
    GpioOutputDriver(GpioOutputDriver&&) = delete;
    GpioOutputDriver& operator=(GpioOutputDriver&&) = delete;

    [[nodiscard]] GpioDriverStatus Initialize();
    [[nodiscard]] GpioDriverStatus ResetAllOutputs();

    [[nodiscard]] GpioDriverStatus WriteLampOutput(
        domain::LampFunction function,
        bool is_active);

    [[nodiscard]] bool IsInitialized() const noexcept;

private:
    bool is_initialized_ {false};
};

}  // namespace body_control::lighting::platform::stm32

#endif  // BODY_CONTROL_LIGHTING_PLATFORM_STM32_GPIO_OUTPUT_DRIVER_HPP_