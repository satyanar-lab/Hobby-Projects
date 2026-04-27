#ifndef IO_BCM_INPUTS_H
#define IO_BCM_INPUTS_H

#include <stdint.h>
#include "platform_types.h"

/*
 * IoBcmInputs_State_t: all *_raw, *_stable, *_latched fields are strictly
 * boolean (0/1 toggle or debounce state), so they use bool_t.
 * Tick fields record a HAL_GetTick() snapshot and must remain uint32_t.
 */
typedef struct
{
    bool_t   head_raw;
    bool_t   head_stable;
    bool_t   head_latched;
    uint32_t head_tick;

    bool_t   park_raw;
    bool_t   park_stable;
    bool_t   park_latched;
    uint32_t park_tick;

    bool_t   left_raw;
    bool_t   left_stable;
    bool_t   left_latched;
    uint32_t left_tick;

    bool_t   right_raw;
    bool_t   right_stable;
    bool_t   right_latched;
    uint32_t right_tick;

    bool_t   hazard_raw;
    bool_t   hazard_stable;
    bool_t   hazard_latched;
    uint32_t hazard_tick;
} IoBcmInputs_State_t;

void    IoBcmInputs_Init(IoBcmInputs_State_t * p_state);
void    IoBcmInputs_Process(IoBcmInputs_State_t * p_state, uint32_t now_ms);
uint8_t IoBcmInputs_GetCommandBits(const IoBcmInputs_State_t * p_state);

#endif /* IO_BCM_INPUTS_H */
