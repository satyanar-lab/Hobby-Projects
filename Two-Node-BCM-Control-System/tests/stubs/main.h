/*
 * stubs/main.h — unified pin-definition stub covering both BCM Master and
 * Lighting Node GPIO assignments.  Replaces both project main.h files for
 * host-side compilation.  All port pointers resolve to valid sentinel
 * addresses; actual values are irrelevant because GPIO calls are stubbed.
 */
#ifndef __MAIN_H
#define __MAIN_H

#include "stm32l1xx_hal.h"

void Error_Handler(void);

/* ----- BCM Master switch inputs (bcm_master/Core/Inc/main.h) ----------- */
#define PARK_LAMP_SW_Pin          GPIO_PIN_0
#define PARK_LAMP_SW_GPIO_Port    GPIOC
#define LEFT_IND_SW_Pin           GPIO_PIN_1
#define LEFT_IND_SW_GPIO_Port     GPIOC
#define RIGHT_IND_SW_Pin          GPIO_PIN_2
#define RIGHT_IND_SW_GPIO_Port    GPIOC
#define HAZARD_SW_Pin             GPIO_PIN_3
#define HAZARD_SW_GPIO_Port       GPIOC
#define HEAD_LAMP_SW_Pin          GPIO_PIN_4
#define HEAD_LAMP_SW_GPIO_Port    GPIOC
#define CAN_CS_Pin                GPIO_PIN_7
#define CAN_CS_GPIO_Port          GPIOC
#define STATUS_LED_Pin            GPIO_PIN_8
#define STATUS_LED_GPIO_Port      GPIOA
#define CAN_INT_Pin               GPIO_PIN_10
#define CAN_INT_GPIO_Port         GPIOA
#define REAR_LINK_LED_Pin         GPIO_PIN_8
#define REAR_LINK_LED_GPIO_Port   GPIOB
#define USER_BTN_Pin              GPIO_PIN_13
#define USER_BTN_GPIO_Port        GPIOC

/* ----- Lighting Node LED outputs (lighting_node/Core/Inc/main.h) -------- */
#define PARK_LAMP_LED_Pin         GPIO_PIN_6
#define PARK_LAMP_LED_GPIO_Port   GPIOC
#define HEAD_LAMP_LED_Pin         GPIO_PIN_8
#define HEAD_LAMP_LED_GPIO_Port   GPIOA
#define HB_LED_Pin                GPIO_PIN_6
#define HB_LED_GPIO_Port          GPIOB
#define LEFT_IND_LED_Pin          GPIO_PIN_7
#define LEFT_IND_LED_GPIO_Port    GPIOB
#define RIGHT_IND_LED_Pin         GPIO_PIN_8
#define RIGHT_IND_LED_GPIO_Port   GPIOB
#define HAZARD_LED_Pin            GPIO_PIN_9
#define HAZARD_LED_GPIO_Port      GPIOB

/* ----- Shared SPI / debug pins ----------------------------------------- */
#define USART_TX_Pin              GPIO_PIN_2
#define USART_TX_GPIO_Port        GPIOA
#define USART_RX_Pin              GPIO_PIN_3
#define USART_RX_GPIO_Port        GPIOA
#define TMS_Pin                   GPIO_PIN_13
#define TMS_GPIO_Port             GPIOA
#define TCK_Pin                   GPIO_PIN_14
#define TCK_GPIO_Port             GPIOA
#define SWO_Pin                   GPIO_PIN_3
#define SWO_GPIO_Port             GPIOB
#define CAN_CS_ALT_Pin            GPIO_PIN_6
#define CAN_CS_ALT_GPIO_Port      GPIOB
#define B1_Pin                    GPIO_PIN_13
#define B1_GPIO_Port              GPIOC

#endif /* __MAIN_H */
