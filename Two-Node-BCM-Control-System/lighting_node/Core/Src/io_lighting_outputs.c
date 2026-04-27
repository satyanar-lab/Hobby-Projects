#include "io_lighting_outputs.h"
#include "gpio.h"
#include "main.h"
#include <stddef.h>

void IoLightingOutputs_Init(IoLightingOutputs_State_t * p_state)
{
    if (p_state != NULL)
    {
        p_state->head_lamp_on = 0U;
        p_state->park_lamp_on = 0U;
        p_state->left_indicator_on = 0U;
        p_state->right_indicator_on = 0U;
        p_state->hazard_lamp_on = 0U;
        p_state->heartbeat_led_on = 0U;
        p_state->state = LIGHTING_STATE_INIT;
    }
}

void IoLightingOutputs_ApplyFailSafe(IoLightingOutputs_State_t * p_state)
{
    if (p_state != NULL)
    {
        p_state->head_lamp_on = 0U;
        p_state->park_lamp_on = 1U;
        p_state->left_indicator_on = 0U;
        p_state->right_indicator_on = 0U;
        p_state->hazard_lamp_on = 0U;
        p_state->state = LIGHTING_STATE_FAIL_SAFE_PARK;
    }
}

void IoLightingOutputs_ApplyCommand(IoLightingOutputs_State_t * p_state,
                                    const Lighting_BcmCommandFrame_t * p_command,
                                    uint8_t indicator_phase)
{
    uint8_t head_command;
    uint8_t park_command;
    uint8_t left_command;
    uint8_t right_command;
    uint8_t hazard_command;

    if ((p_state == NULL) || (p_command == NULL))
    {
        return;
    }

    head_command = ((p_command->command_bits & LIGHTING_CMD_HEAD_LAMP_BIT) != 0U) ? 1U : 0U;
    park_command = ((p_command->command_bits & LIGHTING_CMD_PARK_LAMP_BIT) != 0U) ? 1U : 0U;
    left_command = ((p_command->command_bits & LIGHTING_CMD_LEFT_IND_BIT) != 0U) ? 1U : 0U;
    right_command = ((p_command->command_bits & LIGHTING_CMD_RIGHT_IND_BIT) != 0U) ? 1U : 0U;
    hazard_command = ((p_command->command_bits & LIGHTING_CMD_HAZARD_BIT) != 0U) ? 1U : 0U;

    p_state->head_lamp_on = 0U;
    p_state->park_lamp_on = 0U;
    p_state->left_indicator_on = 0U;
    p_state->right_indicator_on = 0U;
    p_state->hazard_lamp_on = 0U;

    if (head_command == 1U)
    {
        p_state->head_lamp_on = 1U;
    }

    if (hazard_command == 1U)
    {
        p_state->park_lamp_on = 1U;
        if (indicator_phase == 1U)
        {
            p_state->left_indicator_on = 1U;
            p_state->right_indicator_on = 1U;
            p_state->hazard_lamp_on = 1U;
        }
        p_state->state = LIGHTING_STATE_HAZARD_ACTIVE;
    }
    else
    {
        if (park_command == 1U){ p_state->park_lamp_on = 1U; }
        if (left_command == 1U)
        {
            if (indicator_phase == 1U){ p_state->left_indicator_on = 1U; }
            p_state->state = LIGHTING_STATE_LEFT_ACTIVE;
        }
        else if (right_command == 1U)
        {
            if (indicator_phase == 1U){ p_state->right_indicator_on = 1U; }
            p_state->state = LIGHTING_STATE_RIGHT_ACTIVE;
        }
        else if (head_command == 1U)
        {
            p_state->state = LIGHTING_STATE_HEAD_ACTIVE;
        }
        else if (park_command == 1U)
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
        HAL_GPIO_WritePin(HEAD_LAMP_LED_GPIO_Port, HEAD_LAMP_LED_Pin, (p_state->head_lamp_on == 1U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HAL_GPIO_WritePin(PARK_LAMP_LED_GPIO_Port, PARK_LAMP_LED_Pin, (p_state->park_lamp_on == 1U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HAL_GPIO_WritePin(LEFT_IND_LED_GPIO_Port, LEFT_IND_LED_Pin, (p_state->left_indicator_on == 1U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HAL_GPIO_WritePin(RIGHT_IND_LED_GPIO_Port, RIGHT_IND_LED_Pin, (p_state->right_indicator_on == 1U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HAL_GPIO_WritePin(HAZARD_LED_GPIO_Port, HAZARD_LED_Pin, (p_state->hazard_lamp_on == 1U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HAL_GPIO_WritePin(HB_LED_GPIO_Port, HB_LED_Pin, (p_state->heartbeat_led_on == 1U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }
}
