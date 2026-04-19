#include "stm32h7xx_hal.h"
#include "stm32h7xx_it.hpp"

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
    /* Placeholder — HAL_ETH_IRQHandler wired in when ETH HAL is confirmed. */
}

}  /* extern "C" */
