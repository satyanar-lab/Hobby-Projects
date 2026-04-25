#ifndef BODY_CONTROL_LIGHTING_PLATFORM_STM32_NUCLEO_H753ZI_PIN_MAP_HPP_
#define BODY_CONTROL_LIGHTING_PLATFORM_STM32_NUCLEO_H753ZI_PIN_MAP_HPP_

#include <cstdint>

namespace body_control::lighting::platform::stm32
{

/**
 * @brief Logical GPIO port identifiers.
 *
 * These are board-configuration level identifiers. The actual HAL mapping can
 * be done later in the STM32 source implementation.
 */
enum class GpioPortId : std::uint8_t
{
    kPortA = 0U,
    kPortB = 1U,
    kPortC = 2U,
    kPortD = 3U,
    kPortE = 4U,
    kPortF = 5U,
    kPortG = 6U,
    kPortH = 7U,
    kPortI = 8U,
    kUnknown = 255U
};

/**
 * @brief Pin assignment descriptor.
 */
struct PinAssignment
{
    GpioPortId port_id {GpioPortId::kUnknown};
    std::uint16_t pin_number {0U};
};

/**
 * @brief Project lamp output pin assignments.
 *
 * These assignments are project-level placeholders and can be finalized once
 * the exact external wiring for the NUCLEO-H753ZI setup is locked.
 */
namespace pin_map
{

constexpr PinAssignment kLeftIndicatorOutput  {GpioPortId::kPortB, 5U};
constexpr PinAssignment kRightIndicatorOutput {GpioPortId::kPortB, 1U};
constexpr PinAssignment kHazardLampOutput     {GpioPortId::kPortB, 2U};
constexpr PinAssignment kParkLampOutput       {GpioPortId::kPortB, 3U};
constexpr PinAssignment kHeadLampOutput       {GpioPortId::kPortB, 4U};

}  // namespace pin_map

}  // namespace body_control::lighting::platform::stm32

#endif  // BODY_CONTROL_LIGHTING_PLATFORM_STM32_NUCLEO_H753ZI_PIN_MAP_HPP_