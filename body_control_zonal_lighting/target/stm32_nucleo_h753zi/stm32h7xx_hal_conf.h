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
#define HAL_FLASH_MODULE_ENABLED
#define HAL_UART_MODULE_ENABLED

/* ---- Oscillator values assumed by HAL drivers ---------------------------- */
#define HSE_VALUE               8000000U    /* NUCLEO-H753ZI HSE = ST-Link MCO at 8 MHz (BYPASS mode) */
#define HSE_STARTUP_TIMEOUT     100U
#define HSI_VALUE               64000000U
#define CSI_VALUE               4000000U
#define LSI_VALUE               32000U
#define LSE_VALUE               32768U
#define LSE_STARTUP_TIMEOUT     5000U
#define EXTERNAL_CLOCK_VALUE    12288000U

/* ---- VDD / tick ---------------------------------------------------------- */
#define VDD_VALUE               3300U
#define TICK_INT_PRIORITY       15U
#define USE_RTOS                0U
#define PREFETCH_ENABLE         1U

/* ---- ETH ----------------------------------------------------------------- */
#define ETH_SWRESET_TIMEOUT             50U
#define ETH_TX_DESC_CNT         4U
#define ETH_RX_DESC_CNT         4U

/* ---- Assertions ---------------------------------------------------------- */
#define assert_param(expr)      ((void)(expr))

/* ---- Sub-module header includes ------------------------------------------
 * The HAL master header (stm32h7xx_hal.h) relies on stm32h7xx_hal_conf.h
 * to include each enabled module's header, as shown in the CubeH7 conf
 * template (stm32h7xx_hal_conf_template.h lines ~250-400).            */

#include "stm32h7xx_hal_def.h"

#ifdef HAL_RCC_MODULE_ENABLED
  #include "stm32h7xx_hal_rcc.h"
  #include "stm32h7xx_hal_rcc_ex.h"
#endif

#ifdef HAL_GPIO_MODULE_ENABLED
  #include "stm32h7xx_hal_gpio.h"
#endif

#ifdef HAL_DMA_MODULE_ENABLED
  #include "stm32h7xx_hal_dma.h"
  #include "stm32h7xx_hal_dma_ex.h"
#endif

#ifdef HAL_ETH_MODULE_ENABLED
  #include "stm32h7xx_hal_eth.h"
#endif

#ifdef HAL_CORTEX_MODULE_ENABLED
  #include "stm32h7xx_hal_cortex.h"
#endif

#ifdef HAL_PWR_MODULE_ENABLED
  #include "stm32h7xx_hal_pwr.h"
  #include "stm32h7xx_hal_pwr_ex.h"
#endif

#ifdef HAL_FLASH_MODULE_ENABLED
  #include "stm32h7xx_hal_flash.h"
  #include "stm32h7xx_hal_flash_ex.h"
#endif

#ifdef HAL_UART_MODULE_ENABLED
  #include "stm32h7xx_hal_uart.h"
  #include "stm32h7xx_hal_uart_ex.h"
#endif

#endif  /* STM32H7XX_HAL_CONF_H */
