#ifndef APP_LIGHTING_H
#define APP_LIGHTING_H

#include <stdint.h>

typedef struct
{
    uint8_t command_bits;
    uint8_t last_heartbeat;
    uint8_t command_checksum_error;
    uint8_t command_rx_checksum;
    uint8_t command_calc_checksum;
    uint32_t command_checksum_ok_count;
    uint32_t command_checksum_fail_count;
    uint32_t rx_count;
    uint8_t lighting_state;
    uint8_t fail_safe;
    uint8_t heartbeat_timeout;
    uint8_t head_lamp_on;
    uint8_t park_lamp_on;
    uint8_t left_indicator_on;
    uint8_t right_indicator_on;
    uint8_t hazard_lamp_on;
    uint8_t heartbeat_led_on;
    uint8_t status_checksum;
    uint32_t status_tx_count;
} AppLighting_Diag_t;

extern volatile AppLighting_Diag_t g_app_lighting_diag;

void AppLighting_Init(void);
void AppLighting_Start(void);
void AppLighting_RunCycle(void);

#endif /* APP_LIGHTING_H */
