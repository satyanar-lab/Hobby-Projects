#ifndef STM32H7XX_HAL_CONF_H
#define STM32H7XX_HAL_CONF_H

/* ---- HAL module selection ------------------------------------------------ */
#define HAL_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_ETH_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_UART_MODULE_ENABLED

/* ---- Oscillator values assumed by HAL drivers ---------------------------- */
#define HSE_VALUE               8000000U    /* NUCLEO-H753ZI on-board 8 MHz */
#define HSE_STARTUP_TIMEOUT     100U
#define HSI_VALUE               64000000U
#define CSI_VALUE               4000000U
#define LSI_VALUE               32000U
#define LSE_VALUE               32768U
#define LSE_STARTUP_TIMEOUT     5000U
#define EXTERNAL_CLOCK_VALUE    12288000U

/* ---- VDD / tick / SysTick ------------------------------------------------ */
#define VDD_VALUE               3300U
#define TICK_INT_PRIORITY       15U
#define USE_RTOS                0U
#define PREFETCH_ENABLE         1U

/* ---- ETH ----------------------------------------------------------------- */
#define ETH_TX_DESC_CNT         4U
#define ETH_RX_DESC_CNT         4U

/* ---- Assertions ---------------------------------------------------------- */
#define assert_param(expr)      ((void)(expr))

#include "stm32h7xx_hal_def.h"

#endif  /* STM32H7XX_HAL_CONF_H */
