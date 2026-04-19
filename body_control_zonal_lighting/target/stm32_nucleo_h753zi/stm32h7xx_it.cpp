#include "stm32h7xx_hal.h"
#include "stm32h7xx_it.hpp"

extern ETH_HandleTypeDef heth;

extern "C"
{

void NMI_Handler(void)
{
    while (true) {}
}

void HardFault_Handler(void)
{
    while (true) {}
}

void SysTick_Handler(void)
{
    HAL_IncTick();
}

void ETH_IRQHandler(void)
{
    HAL_ETH_IRQHandler(&heth);
}

}  /* extern "C" */
