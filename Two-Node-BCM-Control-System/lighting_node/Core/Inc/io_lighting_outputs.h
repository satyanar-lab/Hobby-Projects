#ifndef IO_LIGHTING_OUTPUTS_H
#define IO_LIGHTING_OUTPUTS_H

#include <stdint.h>
#include "lighting_messages.h"

typedef struct
{
    uint8_t head_lamp_on;
    uint8_t park_lamp_on;
    uint8_t left_indicator_on;
    uint8_t right_indicator_on;
    uint8_t hazard_lamp_on;
    uint8_t heartbeat_led_on;
    uint8_t state;
} IoLightingOutputs_State_t;

void IoLightingOutputs_Init(IoLightingOutputs_State_t * p_state);
void IoLightingOutputs_ApplyFailSafe(IoLightingOutputs_State_t * p_state);
void IoLightingOutputs_ApplyCommand(IoLightingOutputs_State_t * p_state,
                                    const Lighting_BcmCommandFrame_t * p_command,
                                    uint8_t indicator_phase);
void IoLightingOutputs_Write(const IoLightingOutputs_State_t * p_state);

#endif /* IO_LIGHTING_OUTPUTS_H */
