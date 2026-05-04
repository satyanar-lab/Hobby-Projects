// Zephyr GPIO output driver — five lamp pins on NUCLEO-H753ZI.
//
// Logic mirrors the working bare-metal driver in
// src/platform/stm32/gpio_output_driver.cpp.  Hardware access uses the Zephyr
// gpio_dt_spec API instead of the STM32 HAL.
//
// Index discipline
//   kSpecs[] is indexed directly by the LampFunction enum value (1..5).
//   Index 0 (kUnknown) is an empty placeholder so the enum value can be cast
//   straight to the array index without a remapping switch.  This matches the
//   bare-metal LampFunctionToPinAssignment() behaviour: a 1:1 mapping with no
//   off-by-one risk between the enum and the lookup table.
//
// Devicetree
//   GPIO_DT_SPEC_GET(DT_NODELABEL(x), gpios) yields a gpio_dt_spec
//   {port, pin, dt_flags} resolved at link time from the overlay in
//   app/zephyr_nucleo_h753zi/boards/nucleo_h753zi.overlay.

#include "zephyr_gpio_driver.hpp"

#include <cstddef>
#include <cstdint>

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include "body_control/lighting/domain/lamp_command_types.hpp"

LOG_MODULE_REGISTER(zephyr_gpio, LOG_LEVEL_INF);

namespace
{

// Index 0 (kUnknown) is an unused placeholder so kSpecs[func] is a direct
// lookup with no remapping switch.  Indices 1..5 line up exactly with the
// LampFunction enum (kLeftIndicator=1 .. kHeadLamp=5).
static const struct gpio_dt_spec kSpecs[] = {
    [0] = {},                                                          // kUnknown placeholder — index 0 unused
    [1] = GPIO_DT_SPEC_GET(DT_NODELABEL(left_indicator),  gpios),      // kLeftIndicator
    [2] = GPIO_DT_SPEC_GET(DT_NODELABEL(right_indicator), gpios),      // kRightIndicator
    [3] = GPIO_DT_SPEC_GET(DT_NODELABEL(hazard_lamp),     gpios),      // kHazardLamp
    [4] = GPIO_DT_SPEC_GET(DT_NODELABEL(park_lamp),       gpios),      // kParkLamp
    [5] = GPIO_DT_SPEC_GET(DT_NODELABEL(head_lamp),       gpios),      // kHeadLamp
};

static constexpr std::size_t kNumLamps {5U};  // valid enum indices 1..5

}  // namespace

namespace body_control::lighting::platform::zephyr_platform
{

GpioDriverStatus ZephyrGpioDriver::Initialize() noexcept
{
    for (std::size_t idx = 1U; idx <= kNumLamps; ++idx)
    {
        if (!device_is_ready(kSpecs[idx].port))
        {
            LOG_ERR("lamp[%u]: port not ready", static_cast<unsigned>(idx));
            return GpioDriverStatus::kHardwareAccessFailed;
        }

        const int ret = gpio_pin_configure_dt(&kSpecs[idx], GPIO_OUTPUT_INACTIVE);
        if (ret < 0)
        {
            LOG_ERR("lamp[%u]: configure failed (ret=%d)",
                    static_cast<unsigned>(idx), ret);
            return GpioDriverStatus::kHardwareAccessFailed;
        }

        LOG_INF("lamp[%u]: port=%s pin=%d",
                static_cast<unsigned>(idx),
                kSpecs[idx].port->name,
                kSpecs[idx].pin);
    }

    is_initialized_ = true;
    return GpioDriverStatus::kSuccess;
}

GpioDriverStatus ZephyrGpioDriver::ResetAllOutputs() noexcept
{
    if (!is_initialized_) { return GpioDriverStatus::kNotInitialized; }

    for (std::size_t idx = 1U; idx <= kNumLamps; ++idx)
    {
        static_cast<void>(gpio_pin_set_dt(&kSpecs[idx], 0));
    }
    return GpioDriverStatus::kSuccess;
}

GpioDriverStatus ZephyrGpioDriver::WriteLampOutput(
    const domain::LampFunction function,
    const bool                 is_active) noexcept
{
    if (!is_initialized_) { return GpioDriverStatus::kNotInitialized; }

    const auto idx = static_cast<std::uint8_t>(function);
    if ((idx == 0U) || (idx > kNumLamps))
    {
        return GpioDriverStatus::kInvalidArgument;
    }

    const int ret = gpio_pin_set_dt(&kSpecs[idx], is_active ? 1 : 0);

    LOG_INF("GPIO SET lamp=%u pin=%u val=%d",
            static_cast<unsigned>(idx),
            static_cast<unsigned>(kSpecs[idx].pin),
            is_active ? 1 : 0);

    return (ret == 0) ? GpioDriverStatus::kSuccess
                      : GpioDriverStatus::kHardwareAccessFailed;
}

bool ZephyrGpioDriver::IsInitialized() const noexcept
{
    return is_initialized_;
}

}  // namespace body_control::lighting::platform::zephyr_platform
