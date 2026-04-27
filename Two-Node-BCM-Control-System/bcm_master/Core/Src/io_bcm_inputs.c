#include "io_bcm_inputs.h"
#include "main.h"
#include "gpio.h"
#include "lighting_messages.h"
#include <stddef.h>

#define IO_BCM_INPUTS_DEBOUNCE_MS    (30U)

static bool_t IoBcmInputs_ReadActiveLow(GPIO_TypeDef * p_port, uint16_t pin)
{
    bool_t pressed;

    if (HAL_GPIO_ReadPin(p_port, pin) == GPIO_PIN_RESET)
    {
        pressed = TRUE;
    }
    else
    {
        pressed = FALSE;
    }

    return pressed;
}

static bool_t IoBcmInputs_UpdateDebounced(bool_t   sample,
                                          bool_t * p_raw,
                                          bool_t * p_stable,
                                          uint32_t * p_tick,
                                          uint32_t now_ms)
{
    bool_t pressed_event;

    pressed_event = FALSE;

    if (sample != *p_raw)
    {
        *p_raw   = sample;
        *p_tick  = now_ms;
    }

    if ((now_ms - *p_tick) >= IO_BCM_INPUTS_DEBOUNCE_MS)
    {
        if (*p_stable != *p_raw)
        {
            *p_stable = *p_raw;

            if (*p_stable == TRUE)
            {
                pressed_event = TRUE;
            }
        }
    }

    return pressed_event;
}

void IoBcmInputs_Init(IoBcmInputs_State_t * p_state)
{
    if (p_state != NULL)
    {
        p_state->head_raw     = FALSE;
        p_state->head_stable  = FALSE;
        p_state->head_latched = FALSE;
        p_state->head_tick    = 0UL;
        p_state->park_raw     = FALSE;
        p_state->park_stable  = FALSE;
        p_state->park_latched = FALSE;
        p_state->park_tick    = 0UL;
        p_state->left_raw     = FALSE;
        p_state->left_stable  = FALSE;
        p_state->left_latched = FALSE;
        p_state->left_tick    = 0UL;
        p_state->right_raw    = FALSE;
        p_state->right_stable = FALSE;
        p_state->right_latched = FALSE;
        p_state->right_tick   = 0UL;
        p_state->hazard_raw   = FALSE;
        p_state->hazard_stable  = FALSE;
        p_state->hazard_latched = FALSE;
        p_state->hazard_tick  = 0UL;
    }
}

void IoBcmInputs_Process(IoBcmInputs_State_t * p_state, uint32_t now_ms)
{
    bool_t head_sample;
    bool_t park_sample;
    bool_t left_sample;
    bool_t right_sample;
    bool_t hazard_sample;
    bool_t head_event;
    bool_t park_event;
    bool_t left_event;
    bool_t right_event;
    bool_t hazard_event;

    if (p_state == NULL)
    {
        return;
    }

    head_sample   = IoBcmInputs_ReadActiveLow(HEAD_LAMP_SW_GPIO_Port,  HEAD_LAMP_SW_Pin);
    park_sample   = IoBcmInputs_ReadActiveLow(PARK_LAMP_SW_GPIO_Port,  PARK_LAMP_SW_Pin);
    left_sample   = IoBcmInputs_ReadActiveLow(LEFT_IND_SW_GPIO_Port,   LEFT_IND_SW_Pin);
    right_sample  = IoBcmInputs_ReadActiveLow(RIGHT_IND_SW_GPIO_Port,  RIGHT_IND_SW_Pin);
    hazard_sample = IoBcmInputs_ReadActiveLow(HAZARD_SW_GPIO_Port,     HAZARD_SW_Pin);

    head_event   = IoBcmInputs_UpdateDebounced(head_sample,
                                               &p_state->head_raw,
                                               &p_state->head_stable,
                                               &p_state->head_tick,
                                               now_ms);
    park_event   = IoBcmInputs_UpdateDebounced(park_sample,
                                               &p_state->park_raw,
                                               &p_state->park_stable,
                                               &p_state->park_tick,
                                               now_ms);
    hazard_event = IoBcmInputs_UpdateDebounced(hazard_sample,
                                               &p_state->hazard_raw,
                                               &p_state->hazard_stable,
                                               &p_state->hazard_tick,
                                               now_ms);

    if (head_event == TRUE)
    {
        p_state->head_latched = (p_state->head_latched == FALSE) ? TRUE : FALSE;
    }

    if (park_event == TRUE)
    {
        p_state->park_latched = (p_state->park_latched == FALSE) ? TRUE : FALSE;
    }

    if (hazard_event == TRUE)
    {
        if (p_state->hazard_latched == FALSE)
        {
            p_state->hazard_latched = TRUE;
            p_state->left_latched   = FALSE;
            p_state->right_latched  = FALSE;
        }
        else
        {
            p_state->hazard_latched = FALSE;
        }
    }

    if (p_state->hazard_latched == FALSE)
    {
        left_event  = IoBcmInputs_UpdateDebounced(left_sample,
                                                  &p_state->left_raw,
                                                  &p_state->left_stable,
                                                  &p_state->left_tick,
                                                  now_ms);
        right_event = IoBcmInputs_UpdateDebounced(right_sample,
                                                  &p_state->right_raw,
                                                  &p_state->right_stable,
                                                  &p_state->right_tick,
                                                  now_ms);

        if (left_event == TRUE)
        {
            if (p_state->left_latched == FALSE)
            {
                p_state->left_latched  = TRUE;
                p_state->right_latched = FALSE;
            }
            else
            {
                p_state->left_latched = FALSE;
            }
        }

        if (right_event == TRUE)
        {
            if (p_state->right_latched == FALSE)
            {
                p_state->right_latched = TRUE;
                p_state->left_latched  = FALSE;
            }
            else
            {
                p_state->right_latched = FALSE;
            }
        }
    }
    else
    {
        /* Drain debounce timers while hazard is active so indicator state
         * is consistent when hazard is released. */
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
        if (p_state->head_latched == TRUE)
        {
            command_bits |= LIGHTING_CMD_HEAD_LAMP_BIT;
        }
        if (p_state->park_latched == TRUE)
        {
            command_bits |= LIGHTING_CMD_PARK_LAMP_BIT;
        }
        if (p_state->left_latched == TRUE)
        {
            command_bits |= LIGHTING_CMD_LEFT_IND_BIT;
        }
        if (p_state->right_latched == TRUE)
        {
            command_bits |= LIGHTING_CMD_RIGHT_IND_BIT;
        }
        if (p_state->hazard_latched == TRUE)
        {
            command_bits |= LIGHTING_CMD_HAZARD_BIT;
        }
    }

    return command_bits;
}
