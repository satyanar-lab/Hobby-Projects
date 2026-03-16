/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body (Lab 7 - improved accuracy, minimal changes)
  ******************************************************************************
  * Notes:
  *  - PWM: TIM3 CH1
  *  - Direction: PA10 = IN1, PB3 = IN2 (forward set in motor_forward)
  *  - Encoder: PB5 EXTI rising edge (counts pulses)
  *  - ADC: ADC_CHANNEL_1 (PA1/A1) -> potentiometer
  *  - RPM timing: TIM5 free-running counter (PSC=31) used to measure elapsed time
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stm32l1xx_hal.h"
#include "lcd_i2c.h"
#include <stdbool.h>
#include <stdint.h>
/* USER CODE END Includes */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define ENCODER_PPR   1u   // IMPORTANT: change if your encoder has >1 pulse per revolution
/* USER CODE END PD */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc;

I2C_HandleTypeDef hi2c1;

TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim5;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
volatile uint32_t pulse_count = 0;

/* Speed control */
static float duty = 0.15f;            // current applied duty
static float duty_min_run = 0.15f;    // minimum duty to keep spinning (calibrate in Part 2)
static float duty_min_start = 0.20f;  // not used in logic, kept for documentation

/* RPM values */
static uint32_t rpm_expected = 0;
static uint32_t rpm_actual = 0;

/* Timing */
static uint32_t last_rpm_calc_us = 0;

/* Direction (only forward implemented here) */
static bool dir = true;

/* ADC smoothing (EMA) */
static uint32_t adc_filt = 2048;      // filtered ADC value (0..4095)
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_I2C1_Init(void);
static void MX_ADC_Init(void);
static void MX_TIM3_Init(void);
static void MX_TIM5_Init(void);

/* USER CODE BEGIN PFP */
static void motor_forward(void);
static void PWM_SetDuty_TIM3_CH1(float d);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/*
 * Motor forward:
 * PA10 = IN1, PB3 = IN2
 * (Swap motor leads OUT1/OUT2 if CLK/ACLK labeling feels reversed.)
 */
static void motor_forward(void)
{
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3,  GPIO_PIN_RESET);
    dir = true;
}

/*
 * PWM duty update:
 * CCR = duty*(ARR+1) with clamping.
 */
static void PWM_SetDuty_TIM3_CH1(float d)
{
    if (d < 0.0f) d = 0.0f;
    if (d > 1.0f) d = 1.0f;

    uint32_t arr = __HAL_TIM_GET_AUTORELOAD(&htim3);
    uint32_t ccr = (uint32_t)(d * (float)(arr + 1U));

    if (ccr > arr) ccr = arr;

    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, ccr);
}

/*
 * Encoder EXTI callback:
 * PB5 rising edge increments pulse_count
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == GPIO_PIN_5)
    {
        pulse_count++;
    }
}

/* USER CODE END 0 */

int main(void)
{
  HAL_Init();
  SystemClock_Config();

  MX_GPIO_Init();
  MX_USART2_UART_Init();
  MX_I2C1_Init();
  MX_ADC_Init();
  MX_TIM3_Init();
  MX_TIM5_Init();

  /* USER CODE BEGIN 2 */
  RGB_LCD_HandleTypeDef lcd;
  lcd.hi2c = &hi2c1;
  lcd.rgb_addr = 0x62 << 1;
  lcd.cols = 16;
  lcd.rows = 2;

  RGB_LCD_Init(&lcd);
  RGB_LCD_SetRGB(&lcd, 0, 128, 255);
  RGB_LCD_Clear(&lcd);

  motor_forward();

  // Start PWM and timing base
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);

  HAL_TIM_Base_Start(&htim5);
  __HAL_TIM_SET_COUNTER(&htim5, 0);
  last_rpm_calc_us = __HAL_TIM_GET_COUNTER(&htim5);

  // Start ADC (single conversion mode, software-triggered)
  HAL_ADC_Start(&hadc);

  // Apply initial duty
  duty = duty_min_run;
  PWM_SetDuty_TIM3_CH1(duty);

  uint32_t last_disp_ms = HAL_GetTick();
  float expSpd = 0.0f;
  float actlSpd = 0.0f;
  /* USER CODE END 2 */

  while (1)
  {
    uint32_t adc = 0;
    uint32_t now_us = 0;

    /* ---------------- ADC -> PWM (with smoothing) ---------------- */
    if (HAL_ADC_PollForConversion(&hadc, 10) == HAL_OK)
    {
        adc = HAL_ADC_GetValue(&hadc); // 0..4095

        // EMA filter: adc_filt = 7/8 old + 1/8 new
        adc_filt = (adc_filt * 7u + adc) / 8u;

        // Map ADC to [duty_min_run .. 1.0]
        float x = (float)adc_filt / 4095.0f;
        duty = duty_min_run + x * (1.0f - duty_min_run);

        PWM_SetDuty_TIM3_CH1(duty);

        // Better expected RPM model using duty_min_run dead-zone
        if (duty <= duty_min_run)
        {
            rpm_expected = 0;
        }
        else
        {
            float norm = (duty - duty_min_run) / (1.0f - duty_min_run);  // 0..1
            rpm_expected = (uint32_t)(norm * 5000.0f + 0.5f);            // rounded
        }

        // Next conversion
        HAL_ADC_Start(&hadc);
    }

    /* ---------------- RPM calc ~ every 1 second (more accurate) ---------------- */
    now_us = __HAL_TIM_GET_COUNTER(&htim5);
    uint32_t dt_us = (uint32_t)(now_us - last_rpm_calc_us); // handles wrap-around

    if (dt_us >= 1000000UL)
    {
        uint32_t pulses;

        last_rpm_calc_us = now_us;

        __disable_irq();
        pulses = pulse_count;
        pulse_count = 0;
        __enable_irq();

        // RPM = pulses * 60 * 1e6 / (PPR * dt_us)
        uint64_t num = (uint64_t)pulses * 60ULL * 1000000ULL;
        uint64_t den = (uint64_t)ENCODER_PPR * (uint64_t)dt_us;

        rpm_actual = (den > 0ULL) ? (uint32_t)(num / den) : 0u;
    }

    /* ---------------- LCD refresh every 200 ms ---------------- */
    if ((uint32_t)(HAL_GetTick() - last_disp_ms) >= 200U)
    {
        last_disp_ms = HAL_GetTick();

        expSpd = (float)rpm_expected;
        actlSpd = (float)rpm_actual;

        displaySpeed(&lcd, expSpd, actlSpd, dir);
    }

    // Keep loop responsive; timers/interrupts do the real timing
    HAL_Delay(1);
  }
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL6;
  RCC_OscInitStruct.PLL.PLLDIV = RCC_PLL_DIV3;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC_Init(void)
{
  ADC_ChannelConfTypeDef sConfig = {0};

  hadc.Instance = ADC1;
  hadc.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV1;
  hadc.Init.Resolution = ADC_RESOLUTION_12B;
  hadc.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc.Init.EOCSelection = ADC_EOC_SEQ_CONV;
  hadc.Init.LowPowerAutoWait = ADC_AUTOWAIT_DISABLE;
  hadc.Init.LowPowerAutoPowerOff = ADC_AUTOPOWEROFF_DISABLE;
  hadc.Init.ChannelsBank = ADC_CHANNELS_BANK_A;
  hadc.Init.ContinuousConvMode = DISABLE;
  hadc.Init.NbrOfConversion = 1;
  hadc.Init.DiscontinuousConvMode = DISABLE;
  hadc.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc.Init.DMAContinuousRequests = DISABLE;
  if (HAL_ADC_Init(&hadc) != HAL_OK)
  {
    Error_Handler();
  }

  // PA1 / A1
  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_4CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{
  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 0;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 1599;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }

  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_TIM_PWM_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }

  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }

  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }

  HAL_TIM_MspPostInit(&htim3);
}

/**
  * @brief TIM5 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM5_Init(void)
{
  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  htim5.Instance = TIM5;
  htim5.Init.Prescaler = 31;
  htim5.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim5.Init.Period = 4294967295;
  htim5.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim5.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim5) != HAL_OK)
  {
    Error_Handler();
  }

  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim5, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }

  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim5, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3,  GPIO_PIN_RESET);

  /* B1 blue button */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /* PA10 = IN1 */
  GPIO_InitStruct.Pin = GPIO_PIN_10;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* PB3 = IN2 */
  GPIO_InitStruct.Pin = GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* PB5 = encoder input interrupt */
  GPIO_InitStruct.Pin = GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;  // if encoder is open-collector, consider PULLUP
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  HAL_NVIC_SetPriority(EXTI9_5_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
}
#endif /* USE_FULL_ASSERT */
