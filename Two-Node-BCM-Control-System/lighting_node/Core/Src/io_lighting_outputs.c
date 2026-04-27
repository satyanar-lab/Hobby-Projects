#include "io_lighting_outputs.h"
#include "gpio.h"
#include "main.h"
#include <stddef.h>

void IoLightingOutputs_Init(IoLightingOutputs_State_t * p_state)
{
    if (p_state != NULL)
    {
        p_state->head_lamp_on       = FALSE;
        p_state->park_lamp_on       = FALSE;
        p_state->left_indicator_on  = FALSE;
        p_state->right_indicator_on = FALSE;
        p_state->hazard_lamp_on     = FALSE;
        p_state->heartbeat_led_on   = FALSE;
        p_state->state              = LIGHTING_STATE_INIT;
    }
}

void IoLightingOutputs_ApplyFailSafe(IoLightingOutputs_State_t * p_state)
{
    if (p_state != NULL)
    {
        p_state->head_lamp_on       = FALSE;
        p_state->park_lamp_on       = TRUE;   /* park ON is the safe state */
        p_state->left_indicator_on  = FALSE;
        p_state->right_indicator_on = FALSE;
        p_state->hazard_lamp_on     = FALSE;
        p_state->state              = LIGHTING_STATE_FAIL_SAFE_PARK;
    }
}

void IoLightingOutputs_ApplyCommand(IoLightingOutputs_State_t * p_state,
                                    const Lighting_BcmCommandFrame_t * p_command,
                                    bool_t indicator_phase)
{
    bool_t head_command;
    bool_t park_command;
    bool_t left_command;
    bool_t right_command;
    bool_t hazard_command;

    if ((p_state == NULL) || (p_command == NULL))
    {
        return;
    }

    head_command   = ((p_command->command_bits & LIGHTING_CMD_HEAD_LAMP_BIT) != 0U) ? TRUE : FALSE;
    park_command   = ((p_command->command_bits & LIGHTING_CMD_PARK_LAMP_BIT) != 0U) ? TRUE : FALSE;
    left_command   = ((p_command->command_bits & LIGHTING_CMD_LEFT_IND_BIT)  != 0U) ? TRUE : FALSE;
    right_command  = ((p_command->command_bits & LIGHTING_CMD_RIGHT_IND_BIT) != 0U) ? TRUE : FALSE;
    hazard_command = ((p_command->command_bits & LIGHTING_CMD_HAZARD_BIT)    != 0U) ? TRUE : FALSE;

    p_state->head_lamp_on       = FALSE;
    p_state->park_lamp_on       = FALSE;
    p_state->left_indicator_on  = FALSE;
    p_state->right_indicator_on = FALSE;
    p_state->hazard_lamp_on     = FALSE;

    if (head_command == TRUE)
    {
        p_state->head_lamp_on = TRUE;
    }

    if (hazard_command == TRUE)
    {
        /* Hazard overrides individual indicators and forces park ON */
        p_state->park_lamp_on = TRUE;
        if (indicator_phase == TRUE)
        {
            p_state->left_indicator_on  = TRUE;
            p_state->right_indicator_on = TRUE;
            p_state->hazard_lamp_on     = TRUE;
        }
        p_state->state = LIGHTING_STATE_HAZARD_ACTIVE;
    }
    else
    {
        if (park_command == TRUE)
        {
            p_state->park_lamp_on = TRUE;
        }

        if (left_command == TRUE)
        {
            if (indicator_phase == TRUE)
            {
                p_state->left_indicator_on = TRUE;
            }
            p_state->state = LIGHTING_STATE_LEFT_ACTIVE;
        }
        else if (right_command == TRUE)
        {
            if (indicator_phase == TRUE)
            {
                p_state->right_indicator_on = TRUE;
            }
            p_state->state = LIGHTING_STATE_RIGHT_ACTIVE;
        }
        else if (head_command == TRUE)
        {
            p_state->state = LIGHTING_STATE_HEAD_ACTIVE;
        }
        else if (park_command == TRUE)
        {
            p_state->state = LIGHTING_STATE_PARK_ACTIVE;
        }
        else
        {
            p_state->state = LIGHTING_STATE_NORMAL;
        }
    }
}

void IoLightingOutputs_Write(const IoLightingOutputs_State_t * p_state)
{
    if (p_state != NULL)
    {
        HAL_GPIO_WritePin(HEAD_LAMP_LED_GPIO_Port, HEAD_LAMP_LED_Pin,
                          (p_state->head_lamp_on == TRUE) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HAL_GPIO_WritePin(PARK_LAMP_LED_GPIO_Port, PARK_LAMP_LED_Pin,
                          (p_state->park_lamp_on == TRUE) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HAL_GPIO_WritePin(LEFT_IND_LED_GPIO_Port, LEFT_IND_LED_Pin,
                          (p_state->left_indicator_on == TRUE) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HAL_GPIO_WritePin(RIGHT_IND_LED_GPIO_Port, RIGHT_IND_LED_Pin,
                          (p_state->right_indicator_on == TRUE) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HAL_GPIO_WritePin(HAZARD_LED_GPIO_Port, HAZARD_LED_Pin,
                          (p_state->hazard_lamp_on == TRUE) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HAL_GPIO_WritePin(HB_LED_GPIO_Port, HB_LED_Pin,
                          (p_state->heartbeat_led_on == TRUE) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }
}
