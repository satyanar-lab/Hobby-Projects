#ifndef IO_BCM_INPUTS_H
#define IO_BCM_INPUTS_H

#include <stdint.h>

typedef struct
{
    uint8_t head_raw;
    uint8_t head_stable;
    uint8_t head_latched;
    uint32_t head_tick;

    uint8_t park_raw;
    uint8_t park_stable;
    uint8_t park_latched;
    uint32_t park_tick;

    uint8_t left_raw;
    uint8_t left_stable;
    uint8_t left_latched;
    uint32_t left_tick;

    uint8_t right_raw;
    uint8_t right_stable;
    uint8_t right_latched;
    uint32_t right_tick;

    uint8_t hazard_raw;
    uint8_t hazard_stable;
    uint8_t hazard_latched;
    uint32_t hazard_tick;
} IoBcmInputs_State_t;

void IoBcmInputs_Init(IoBcmInputs_State_t * p_state);
void IoBcmInputs_Process(IoBcmInputs_State_t * p_state, uint32_t now_ms);
uint8_t IoBcmInputs_GetCommandBits(const IoBcmInputs_State_t * p_state);

#endif /* IO_BCM_INPUTS_H */
