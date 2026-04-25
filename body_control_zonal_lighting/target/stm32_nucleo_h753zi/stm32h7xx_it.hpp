#ifndef STM32H7XX_IT_HPP_
#define STM32H7XX_IT_HPP_

#ifdef __cplusplus
extern "C" {
#endif

void NMI_Handler(void);
void HardFault_Handler(void);
void MemManage_Handler(void);
void BusFault_Handler(void);
void UsageFault_Handler(void);
void SysTick_Handler(void);
void ETH_IRQHandler(void);

#ifdef __cplusplus
}
#endif

#endif  /* STM32H7XX_IT_HPP_ */
