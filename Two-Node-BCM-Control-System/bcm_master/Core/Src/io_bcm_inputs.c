#include "io_bcm_inputs.h"
#include "main.h"
#include "gpio.h"
#include "lighting_messages.h"
#include <stddef.h>

#define IO_BCM_INPUTS_DEBOUNCE_MS    (30U)

static uint8_t IoBcmInputs_ReadActiveLow(GPIO_TypeDef * p_port, uint16_t pin)
{
    uint8_t pressed;

    if (HAL_GPIO_ReadPin(p_port, pin) == GPIO_PIN_RESET)
    {
        pressed = 1U;
    }
    else
    {
        pressed = 0U;
    }

    return pressed;
}

static uint8_t IoBcmInputs_UpdateDebounced(uint8_t sample,
                                           uint8_t * p_raw,
                                           uint8_t * p_stable,
                                           uint32_t * p_tick,
                                           uint32_t now_ms)
{
    uint8_t pressed_event;

    pressed_event = 0U;

    if (sample != *p_raw)
    {
        *p_raw = sample;
        *p_tick = now_ms;
    }

    if ((now_ms - *p_tick) >= IO_BCM_INPUTS_DEBOUNCE_MS)
    {
        if (*p_stable != *p_raw)
        {
            *p_stable = *p_raw;

            if (*p_stable == 1U)
            {
                pressed_event = 1U;
            }
        }
    }

    return pressed_event;
}

void IoBcmInputs_Init(IoBcmInputs_State_t * p_state)
{
    if (p_state != NULL)
    {
        p_state->head_raw = 0U;
        p_state->head_stable = 0U;
        p_state->head_latched = 0U;
        p_state->head_tick = 0UL;
        p_state->park_raw = 0U;
        p_state->park_stable = 0U;
        p_state->park_latched = 0U;
        p_state->park_tick = 0UL;
        p_state->left_raw = 0U;
        p_state->left_stable = 0U;
        p_state->left_latched = 0U;
        p_state->left_tick = 0UL;
        p_state->right_raw = 0U;
        p_state->right_stable = 0U;
        p_state->right_latched = 0U;
        p_state->right_tick = 0UL;
        p_state->hazard_raw = 0U;
        p_state->hazard_stable = 0U;
        p_state->hazard_latched = 0U;
        p_state->hazard_tick = 0UL;
    }
}

void IoBcmInputs_Process(IoBcmInputs_State_t * p_state, uint32_t now_ms)
{
    uint8_t head_sample;
    uint8_t park_sample;
    uint8_t left_sample;
    uint8_t right_sample;
    uint8_t hazard_sample;
    uint8_t head_event;
    uint8_t park_event;
    uint8_t left_event;
    uint8_t right_event;
    uint8_t hazard_event;

    if (p_state == NULL)
    {
        return;
    }

    head_sample = IoBcmInputs_ReadActiveLow(HEAD_LAMP_SW_GPIO_Port, HEAD_LAMP_SW_Pin);
    park_sample = IoBcmInputs_ReadActiveLow(PARK_LAMP_SW_GPIO_Port, PARK_LAMP_SW_Pin);
    left_sample = IoBcmInputs_ReadActiveLow(LEFT_IND_SW_GPIO_Port, LEFT_IND_SW_Pin);
    right_sample = IoBcmInputs_ReadActiveLow(RIGHT_IND_SW_GPIO_Port, RIGHT_IND_SW_Pin);
    hazard_sample = IoBcmInputs_ReadActiveLow(HAZARD_SW_GPIO_Port, HAZARD_SW_Pin);

    head_event = IoBcmInputs_UpdateDebounced(head_sample,
                                             &p_state->head_raw,
                                             &p_state->head_stable,
                                             &p_state->head_tick,
                                             now_ms);
    park_event = IoBcmInputs_UpdateDebounced(park_sample,
                                             &p_state->park_raw,
                                             &p_state->park_stable,
                                             &p_state->park_tick,
                                             now_ms);
    hazard_event = IoBcmInputs_UpdateDebounced(hazard_sample,
                                               &p_state->hazard_raw,
                                               &p_state->hazard_stable,
                                               &p_state->hazard_tick,
                                               now_ms);

    if (head_event == 1U)
    {
        p_state->head_latched ^= 1U;
    }

    if (park_event == 1U)
    {
        p_state->park_latched ^= 1U;
    }

    if (hazard_event == 1U)
    {
        if (p_state->hazard_latched == 0U)
        {
            p_state->hazard_latched = 1U;
            p_state->left_latched = 0U;
            p_state->right_latched = 0U;
        }
        else
        {
            p_state->hazard_latched = 0U;
        }
    }

    if (p_state->hazard_latched == 0U)
    {
        left_event = IoBcmInputs_UpdateDebounced(left_sample,
                                                 &p_state->left_raw,
                                                 &p_state->left_stable,
                                                 &p_state->left_tick,
                                                 now_ms);
        right_event = IoBcmInputs_UpdateDebounced(right_sample,
                                                  &p_state->right_raw,
                                                  &p_state->right_stable,
                                                  &p_state->right_tick,
                                                  now_ms);

        if (left_event == 1U)
        {
            if (p_state->left_latched == 0U)
            {
                p_state->left_latched = 1U;
                p_state->right_latched = 0U;
            }
            else
            {
                p_state->left_latched = 0U;
            }
        }

        if (right_event == 1U)
        {
            if (p_state->right_latched == 0U)
            {
                p_state->right_latched = 1U;
                p_state->left_latched = 0U;
            }
            else
            {
                p_state->right_latched = 0U;
            }
        }
    }
    else
    {
        (void)IoBcmInputs_UpdateDebounced(left_sample,
                                          &p_state->left_raw,
                                          &p_state->left_stable,
                                          &p_state->left_tick,
                                          now_ms);
        (void)IoBcmInputs_UpdateDebounced(right_sample,
                                          &p_state->right_raw,
                                          &p_state->right_stable,
                                          &p_state->right_tick,
                                          now_ms);
    }
}

uint8_t IoBcmInputs_GetCommandBits(const IoBcmInputs_State_t * p_state)
{
    uint8_t command_bits;

    command_bits = 0U;

    if (p_state != NULL)
    {
        if (p_state->head_latched == 1U)
        {
            command_bits |= LIGHTING_CMD_HEAD_LAMP_BIT;
        }

        if (p_state->park_latched == 1U)
        {
            command_bits |= LIGHTING_CMD_PARK_LAMP_BIT;
        }

        if (p_state->left_latched == 1U)
        {
            command_bits |= LIGHTING_CMD_LEFT_IND_BIT;
        }

        if (p_state->right_latched == 1U)
        {
            command_bits |= LIGHTING_CMD_RIGHT_IND_BIT;
        }

        if (p_state->hazard_latched == 1U)
        {
            command_bits |= LIGHTING_CMD_HAZARD_BIT;
        }
    }

    return command_bits;
}
