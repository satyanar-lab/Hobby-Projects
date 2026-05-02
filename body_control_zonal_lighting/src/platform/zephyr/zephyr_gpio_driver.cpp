#include "zephyr_gpio_driver.hpp"

#include <cstddef>

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>

#include "body_control/lighting/domain/lamp_command_types.hpp"

namespace
{

// Pin specs resolved at compile/link time from the DT overlay.
// GPIO_DT_SPEC_GET(DT_NODELABEL(x), gpios) yields a gpio_dt_spec
// {port=&__device_dts_ord_N, pin=P, dt_flags=F} — a linker-time constant.
// Array index maps directly to LampFunctionToIndex() below.
static const struct gpio_dt_spec kLampPins[] = {
    GPIO_DT_SPEC_GET(DT_NODELABEL(left_indicator),  gpios),  // index 0
    GPIO_DT_SPEC_GET(DT_NODELABEL(right_indicator), gpios),  // index 1
    GPIO_DT_SPEC_GET(DT_NODELABEL(hazard_lamp),     gpios),  // index 2
    GPIO_DT_SPEC_GET(DT_NODELABEL(park_lamp),       gpios),  // index 3
    GPIO_DT_SPEC_GET(DT_NODELABEL(head_lamp),       gpios),  // index 4
};

constexpr std::size_t kPinCount {5U};

std::size_t LampFunctionToIndex(
    const body_control::lighting::domain::LampFunction function) noexcept
{
    using LF = body_control::lighting::domain::LampFunction;
    switch (function)
    {
        case LF::kLeftIndicator:  return 0U;
        case LF::kRightIndicator: return 1U;
        case LF::kHazardLamp:     return 2U;
        case LF::kParkLamp:       return 3U;
        case LF::kHeadLamp:       return 4U;
        default:                  return kPinCount;  // sentinel: invalid index
    }
}

}  // namespace

namespace body_control::lighting::platform::zephyr_platform
{

GpioDriverStatus ZephyrGpioDriver::Initialize() noexcept
{
    for (const auto& spec : kLampPins)
    {
        if (!device_is_ready(spec.port))
        {
            return GpioDriverStatus::kHardwareAccessFailed;
        }
        if (gpio_pin_configure_dt(&spec, GPIO_OUTPUT_INACTIVE) < 0)
        {
            return GpioDriverStatus::kHardwareAccessFailed;
        }
    }
    is_initialized_ = true;
    return GpioDriverStatus::kSuccess;
}

GpioDriverStatus ZephyrGpioDriver::ResetAllOutputs() noexcept
{
    if (!is_initialized_) { return GpioDriverStatus::kNotInitialized; }

    for (const auto& spec : kLampPins)
    {
        static_cast<void>(gpio_pin_set_dt(&spec, 0));
    }
    return GpioDriverStatus::kSuccess;
}

GpioDriverStatus ZephyrGpioDriver::WriteLampOutput(
    const domain::LampFunction function,
    const bool is_active) noexcept
{
    if (!is_initialized_) { return GpioDriverStatus::kNotInitialized; }

    const std::size_t idx = LampFunctionToIndex(function);
    if (idx >= kPinCount)    { return GpioDriverStatus::kInvalidArgument; }

    const int ret = gpio_pin_set_dt(&kLampPins[idx], is_active ? 1 : 0);
    return (ret == 0) ? GpioDriverStatus::kSuccess
                      : GpioDriverStatus::kHardwareAccessFailed;
}

bool ZephyrGpioDriver::IsInitialized() const noexcept
{
    return is_initialized_;
}

}  // namespace body_control::lighting::platform::zephyr_platform
