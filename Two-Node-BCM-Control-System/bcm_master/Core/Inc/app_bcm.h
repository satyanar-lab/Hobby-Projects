#ifndef APP_BCM_H
#define APP_BCM_H

#include <stdint.h>
#include "platform_types.h"

typedef struct
{
    uint8_t  command_bits;         /* bitmask — data, not bool */
    uint8_t  heartbeat_counter;    /* counter — data, not bool */
    uint8_t  command_checksum;     /* CRC-8 value — data, not bool */
    uint32_t tx_count;
    bool_t   lighting_status_timeout;
    uint8_t  lighting_state;       /* LIGHTING_STATE_* enum — not bool */
    bool_t   lighting_fail_safe;
    bool_t   lighting_hb_timeout;
    bool_t   lighting_head_active;
    bool_t   lighting_park_active;
    bool_t   lighting_left_active;
    bool_t   lighting_right_active;
    bool_t   lighting_hazard_active;
    bool_t   status_checksum_error;
    uint8_t  status_rx_checksum;   /* CRC-8 value — data, not bool */
    uint8_t  status_calc_checksum; /* CRC-8 value — data, not bool */
    uint32_t status_checksum_ok_count;
    uint32_t status_checksum_fail_count;
    bool_t   link_led_state;
    uint32_t link_led_blink_count;
} AppBcm_Diag_t;

extern volatile AppBcm_Diag_t g_app_bcm_diag;

void AppBcm_Init(void);
void AppBcm_Start(void);
void AppBcm_RunCycle(void);

#endif /* APP_BCM_H */
