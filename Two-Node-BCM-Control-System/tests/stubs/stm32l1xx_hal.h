/*
 * stubs/stm32l1xx_hal.h — minimal STM32 HAL stub for host-side unit tests.
 * Provides only the types and macros needed by io_bcm_inputs.c,
 * io_lighting_outputs.c, and their headers.  The actual HAL functions
 * (GPIO read/write, GetTick, Delay) are implemented in test_runner.c.
 */
#ifndef STM32L1XX_HAL_H
#define STM32L1XX_HAL_H

#include <stdint.h>

typedef enum
{
    HAL_OK      = 0x00U,
    HAL_ERROR   = 0x01U,
    HAL_BUSY    = 0x02U,
    HAL_TIMEOUT = 0x03U
} HAL_StatusTypeDef;

typedef struct { uint32_t dummy; } GPIO_TypeDef;

typedef enum { GPIO_PIN_RESET = 0, GPIO_PIN_SET = 1 } GPIO_PinState;

#define GPIO_PIN_0   ((uint16_t)0x0001U)
#define GPIO_PIN_1   ((uint16_t)0x0002U)
#define GPIO_PIN_2   ((uint16_t)0x0004U)
#define GPIO_PIN_3   ((uint16_t)0x0008U)
#define GPIO_PIN_4   ((uint16_t)0x0010U)
#define GPIO_PIN_5   ((uint16_t)0x0020U)
#define GPIO_PIN_6   ((uint16_t)0x0040U)
#define GPIO_PIN_7   ((uint16_t)0x0080U)
#define GPIO_PIN_8   ((uint16_t)0x0100U)
#define GPIO_PIN_9   ((uint16_t)0x0200U)
#define GPIO_PIN_10  ((uint16_t)0x0400U)
#define GPIO_PIN_11  ((uint16_t)0x0800U)
#define GPIO_PIN_12  ((uint16_t)0x1000U)
#define GPIO_PIN_13  ((uint16_t)0x2000U)
#define GPIO_PIN_14  ((uint16_t)0x4000U)
#define GPIO_PIN_15  ((uint16_t)0x8000U)

/* Peripheral base addresses are irrelevant in stubs — use sentinel values */
#define GPIOA  ((GPIO_TypeDef *)0x40020000UL)
#define GPIOB  ((GPIO_TypeDef *)0x40020400UL)
#define GPIOC  ((GPIO_TypeDef *)0x40020800UL)

/* Function prototypes — implementations provided in test_runner.c */
uint32_t       HAL_GetTick(void);
void           HAL_Delay(uint32_t ms);
GPIO_PinState  HAL_GPIO_ReadPin(GPIO_TypeDef *port, uint16_t pin);
void           HAL_GPIO_WritePin(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState state);

#endif /* STM32L1XX_HAL_H */
