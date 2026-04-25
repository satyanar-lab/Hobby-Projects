// HAL MSP (MCU-Specific Package) callbacks for NUCLEO-H753ZI.
//
// HAL_Init calls HAL_MspInit; HAL_UART_Init calls HAL_UART_MspInit;
// HAL_ETH_Init calls HAL_ETH_MspInit.  Without these overrides the weak
// HAL stubs do nothing — clocks are disabled and pins float.

#include "stm32h7xx_hal.h"

// Called from HAL_Init — runs before any peripheral is touched.
// D2SRAM clocks must be enabled here so that subsequent accesses to D2-domain
// peripherals (ETH, USART3) do not fault on the first AHB1 bus transaction.
void HAL_MspInit(void)
{
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    __HAL_RCC_D2SRAM1_CLK_ENABLE();
    __HAL_RCC_D2SRAM2_CLK_ENABLE();
    __HAL_RCC_D2SRAM3_CLK_ENABLE();
    // DMA1 on AHB1 — primes the D2 AHB1 bus fabric before ETH registers are touched.
    __HAL_RCC_DMA1_CLK_ENABLE();
}

// Called from HAL_UART_Init — configures USART3 clock and ST-Link VCP pins.
// NUCLEO-H753ZI ST-Link VCP: USART3, PD8 TX, PD9 RX, AF7.
void HAL_UART_MspInit(UART_HandleTypeDef* huart)
{
    if (huart->Instance != USART3) { return; }

    __HAL_RCC_USART3_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    GPIO_InitTypeDef gpio{};
    gpio.Pin       = GPIO_PIN_8 | GPIO_PIN_9;  // PD8 = TX, PD9 = RX
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_PULLUP;
    gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF7_USART3;
    HAL_GPIO_Init(GPIOD, &gpio);
}

// Called from HAL_ETH_Init.
// NUCLEO-H753ZI RMII wiring (matches reference NUCLEO-H743ZI MB1364):
//   PA1 REF_CLK, PA2 MDIO, PA7 CRS_DV
//   PB13 TXD1
//   PC1 MDC, PC4 RXD0, PC5 RXD1
//   PG2 RXER, PG11 TX_EN, PG13 TXD0
void HAL_ETH_MspInit(ETH_HandleTypeDef* const heth)
{
    if ((heth == nullptr) || (heth->Instance != ETH)) { return; }

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();

    GPIO_InitTypeDef gpio{};
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_HIGH;
    gpio.Alternate = GPIO_AF11_ETH;

    gpio.Pin = GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_7;
    HAL_GPIO_Init(GPIOA, &gpio);

    gpio.Pin = GPIO_PIN_13;
    HAL_GPIO_Init(GPIOB, &gpio);

    gpio.Pin = GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5;
    HAL_GPIO_Init(GPIOC, &gpio);

    gpio.Pin = GPIO_PIN_2 | GPIO_PIN_11 | GPIO_PIN_13;
    HAL_GPIO_Init(GPIOG, &gpio);

    __HAL_RCC_SYSCFG_CLK_ENABLE();
    __HAL_RCC_ETH1MAC_CLK_ENABLE();
    __HAL_RCC_ETH1TX_CLK_ENABLE();
    __HAL_RCC_ETH1RX_CLK_ENABLE();

    // Select RMII interface mode in SYSCFG before ETH MAC registers are touched.
    SYSCFG->PMCR = (SYSCFG->PMCR & ~SYSCFG_PMCR_EPIS_SEL_Msk)
                 | SYSCFG_PMCR_EPIS_SEL_2;

    // ETH_IRQn not enabled — firmware uses polling HAL_ETH_Start, not Start_IT.
}

void HAL_ETH_MspDeInit(ETH_HandleTypeDef* /*heth*/)
{
    __HAL_RCC_ETH1MAC_CLK_DISABLE();
    __HAL_RCC_ETH1TX_CLK_DISABLE();
    __HAL_RCC_ETH1RX_CLK_DISABLE();

    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_7);
    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_13);
    HAL_GPIO_DeInit(GPIOC, GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5);
    HAL_GPIO_DeInit(GPIOG, GPIO_PIN_2 | GPIO_PIN_11 | GPIO_PIN_13);
}
