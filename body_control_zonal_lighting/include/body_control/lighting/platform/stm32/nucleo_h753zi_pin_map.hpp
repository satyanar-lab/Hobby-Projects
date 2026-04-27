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

/** Lamp output pin assignments for the NUCLEO-H753ZI demo wiring.
 *
 *  All five functions are mapped to GPIOB pins 1–5 to allow a single
 *  GPIO_InitTypeDef bitmask in GpioOutputDriver::Initialize().  Adjust
 *  these constants if the physical wiring changes; no other code needs
 *  to be touched because GpioOutputDriver looks up pins via this map. */
namespace pin_map
{

constexpr PinAssignment kLeftIndicatorOutput  {GpioPortId::kPortB, 5U}; ///< PB5
constexpr PinAssignment kRightIndicatorOutput {GpioPortId::kPortB, 1U}; ///< PB1
constexpr PinAssignment kHazardLampOutput     {GpioPortId::kPortB, 2U}; ///< PB2
constexpr PinAssignment kParkLampOutput       {GpioPortId::kPortB, 3U}; ///< PB3
constexpr PinAssignment kHeadLampOutput       {GpioPortId::kPortB, 4U}; ///< PB4

}  // namespace pin_map

}  // namespace body_control::lighting::platform::stm32

#endif  // BODY_CONTROL_LIGHTING_PLATFORM_STM32_NUCLEO_H753ZI_PIN_MAP_HPP_