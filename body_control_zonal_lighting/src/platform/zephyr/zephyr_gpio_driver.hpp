#ifndef BODY_CONTROL_LIGHTING_PLATFORM_ZEPHYR_GPIO_DRIVER_HPP_
#define BODY_CONTROL_LIGHTING_PLATFORM_ZEPHYR_GPIO_DRIVER_HPP_

#include <cstdint>

#include "body_control/lighting/domain/lamp_command_types.hpp"

namespace body_control::lighting::platform::zephyr_platform
{

/** Return status for Zephyr GPIO driver operations.
 *
 *  Mirrors the stm32 GpioDriverStatus values so callers can handle both
 *  platforms uniformly.  Defined separately so this header has no dependency
 *  on the stm32 platform headers. */
enum class GpioDriverStatus : std::uint8_t
{
    kSuccess              = 0U,  ///< Pin write completed without error.
    kNotInitialized       = 1U,  ///< Initialize() has not been called.
    kInvalidArgument      = 2U,  ///< LampFunction is kUnknown or has no DT mapping.
    kHardwareAccessFailed = 3U   ///< gpio_pin_configure_dt or gpio_pin_set_dt failed.
};

/** Zephyr GPIO output driver for the five lamp control pins.
 *
 *  Pin assignments are read from the devicetree at compile time via
 *  GPIO_DT_SPEC_GET(DT_NODELABEL(...), gpios) in the implementation file.
 *  The five DT node labels (left_indicator, right_indicator, hazard_lamp,
 *  park_lamp, head_lamp) must be defined in boards/nucleo_h753zi.overlay.
 *
 *  Initialize() calls gpio_pin_configure_dt() for all five pins.
 *  WriteLampOutput() calls gpio_pin_set_dt() for the named function. */
class ZephyrGpioDriver
{
public:
    ZephyrGpioDriver() noexcept = default;
    ~ZephyrGpioDriver() = default;

    ZephyrGpioDriver(const ZephyrGpioDriver&)            = delete;
    ZephyrGpioDriver& operator=(const ZephyrGpioDriver&) = delete;
    ZephyrGpioDriver(ZephyrGpioDriver&&)                 = delete;
    ZephyrGpioDriver& operator=(ZephyrGpioDriver&&)      = delete;

    /** Configures all five lamp pins as push-pull outputs, initially inactive. */
    [[nodiscard]] GpioDriverStatus Initialize() noexcept;

    /** Drives all five lamp pins inactive (lamps off). */
    [[nodiscard]] GpioDriverStatus ResetAllOutputs() noexcept;

    /** Sets the GPIO pin for function to active (high) or inactive (low). */
    [[nodiscard]] GpioDriverStatus WriteLampOutput(
        domain::LampFunction function,
        bool is_active) noexcept;

    [[nodiscard]] bool IsInitialized() const noexcept;

private:
    bool is_initialized_ {false};
};

}  // namespace body_control::lighting::platform::zephyr_platform

#endif  // BODY_CONTROL_LIGHTING_PLATFORM_ZEPHYR_GPIO_DRIVER_HPP_
