#ifndef LIGHTING_MESSAGES_H
#define LIGHTING_MESSAGES_H

#include <stdint.h>
#include "platform_types.h"

/* Message identifiers */
#define LIGHTING_BCM_COMMAND_ID         (0x123U)
#define LIGHTING_STATUS_ID              (0x124U)

/* Common frame settings */
#define LIGHTING_FRAME_LENGTH           (16U)
#define LIGHTING_FRAME_CHECKSUM_INDEX   (15U)

/* BCM command bits */
#define LIGHTING_CMD_HEAD_LAMP_BIT      (0x01U)
#define LIGHTING_CMD_PARK_LAMP_BIT      (0x02U)
#define LIGHTING_CMD_LEFT_IND_BIT       (0x04U)
#define LIGHTING_CMD_RIGHT_IND_BIT      (0x08U)
#define LIGHTING_CMD_HAZARD_BIT         (0x10U)

/* Lighting status flags */
#define LIGHTING_STATUS_HEAD_ACTIVE_BIT     (0x01U)
#define LIGHTING_STATUS_PARK_ACTIVE_BIT     (0x02U)
#define LIGHTING_STATUS_LEFT_ACTIVE_BIT     (0x04U)
#define LIGHTING_STATUS_RIGHT_ACTIVE_BIT    (0x08U)
#define LIGHTING_STATUS_HAZARD_ACTIVE_BIT   (0x10U)
#define LIGHTING_STATUS_HB_TIMEOUT_BIT      (0x20U)
#define LIGHTING_STATUS_FAIL_SAFE_BIT       (0x40U)

/* Lighting state values */
#define LIGHTING_STATE_INIT             (0U)
#define LIGHTING_STATE_NORMAL           (1U)
#define LIGHTING_STATE_HEAD_ACTIVE      (2U)
#define LIGHTING_STATE_PARK_ACTIVE      (3U)
#define LIGHTING_STATE_LEFT_ACTIVE      (4U)
#define LIGHTING_STATE_RIGHT_ACTIVE     (5U)
#define LIGHTING_STATE_HAZARD_ACTIVE    (6U)
#define LIGHTING_STATE_FAIL_SAFE_PARK   (7U)

/*
 * Wire-format structs: all fields map 1-to-1 to CAN FD payload bytes and
 * must therefore remain uint8_t regardless of their boolean semantics.
 * Do not change field types here; use bool_t only in application-layer
 * context and diagnostic structs that are never serialised.
 */
typedef struct
{
    uint8_t command_bits;
    uint8_t heartbeat_counter;
    uint8_t sender_node_id;
    uint8_t bcm_alive;
    uint8_t rolling_counter;
    uint8_t reserved_5;
    uint8_t reserved_6;
    uint8_t reserved_7;
    uint8_t reserved_8;
    uint8_t reserved_9;
    uint8_t reserved_10;
    uint8_t reserved_11;
    uint8_t reserved_12;
    uint8_t reserved_13;
    uint8_t reserved_14;
    uint8_t checksum;
} Lighting_BcmCommandFrame_t;

typedef struct
{
    uint8_t flags;
    uint8_t state;
    uint8_t last_heartbeat_rx;
    uint8_t status_counter;
    uint8_t command_bits_echo;
    uint8_t rx_count_lsb;
    uint8_t heartbeat_led_state;
    uint8_t indicator_blink_state;
    uint8_t head_command_echo;
    uint8_t park_command_echo;
    uint8_t left_command_echo;
    uint8_t right_command_echo;
    uint8_t hazard_command_echo;
    uint8_t diagnostic_pattern_a5;
    uint8_t node_id;
    uint8_t checksum;
} Lighting_StatusFrame_t;

#endif /* LIGHTING_MESSAGES_H */
