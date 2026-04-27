#ifndef IO_LIGHTING_OUTPUTS_H
#define IO_LIGHTING_OUTPUTS_H

#include <stdint.h>
#include "platform_types.h"
#include "lighting_messages.h"

/*
 * IoLightingOutputs_State_t: *_on fields are strictly boolean (lamp driven
 * or not), so they use bool_t.  state holds an enum value (LIGHTING_STATE_*)
 * and must remain uint8_t.
 */
typedef struct
{
    bool_t  head_lamp_on;
    bool_t  park_lamp_on;
    bool_t  left_indicator_on;
    bool_t  right_indicator_on;
    bool_t  hazard_lamp_on;
    bool_t  heartbeat_led_on;
    uint8_t state;
} IoLightingOutputs_State_t;

void IoLightingOutputs_Init(IoLightingOutputs_State_t * p_state);
void IoLightingOutputs_ApplyFailSafe(IoLightingOutputs_State_t * p_state);
void IoLightingOutputs_ApplyCommand(IoLightingOutputs_State_t * p_state,
                                    const Lighting_BcmCommandFrame_t * p_command,
                                    bool_t indicator_phase);
void IoLightingOutputs_Write(const IoLightingOutputs_State_t * p_state);

#endif /* IO_LIGHTING_OUTPUTS_H */
