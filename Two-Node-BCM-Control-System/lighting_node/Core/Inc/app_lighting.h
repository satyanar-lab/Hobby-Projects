#ifndef APP_LIGHTING_H
#define APP_LIGHTING_H

#include <stdint.h>
#include "platform_types.h"

typedef struct
{
    uint8_t  command_bits;              /* bitmask — data, not bool */
    uint8_t  last_heartbeat;            /* counter value — data, not bool */
    bool_t   command_checksum_error;
    uint8_t  command_rx_checksum;       /* CRC-8 value — data, not bool */
    uint8_t  command_calc_checksum;     /* CRC-8 value — data, not bool */
    uint32_t command_checksum_ok_count;
    uint32_t command_checksum_fail_count;
    uint32_t rx_count;
    uint8_t  lighting_state;            /* LIGHTING_STATE_* enum — not bool */
    bool_t   fail_safe;
    bool_t   heartbeat_timeout;
    bool_t   head_lamp_on;
    bool_t   park_lamp_on;
    bool_t   left_indicator_on;
    bool_t   right_indicator_on;
    bool_t   hazard_lamp_on;
    bool_t   heartbeat_led_on;
    uint8_t  status_checksum;           /* CRC-8 value — data, not bool */
    uint32_t status_tx_count;
} AppLighting_Diag_t;

extern volatile AppLighting_Diag_t g_app_lighting_diag;

void AppLighting_Init(void);
void AppLighting_Start(void);
void AppLighting_RunCycle(void);

#endif /* APP_LIGHTING_H */
