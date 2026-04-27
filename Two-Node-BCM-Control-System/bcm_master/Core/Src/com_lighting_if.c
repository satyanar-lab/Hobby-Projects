#include "com_lighting_if.h"
#include <stddef.h>

uint8_t ComLighting_CalculateChecksum(const uint8_t * p_data)
{
    uint8_t checksum;
    uint8_t index;

    checksum = 0U;

    if (p_data != NULL)
    {
        for (index = 0U; index < LIGHTING_FRAME_CHECKSUM_INDEX; index++)
        {
            checksum ^= p_data[index];
        }
    }

    return checksum;
}

void ComLighting_EncodeBcmCommand(const Lighting_BcmCommandFrame_t * p_frame,
                                  uint8_t * p_data)
{
    if ((p_frame == NULL) || (p_data == NULL))
    {
        return;
    }

    p_data[0] = p_frame->command_bits;
    p_data[1] = p_frame->heartbeat_counter;
    p_data[2] = p_frame->sender_node_id;
    p_data[3] = p_frame->bcm_alive;
    p_data[4] = p_frame->rolling_counter;
    p_data[5] = p_frame->reserved_5;
    p_data[6] = p_frame->reserved_6;
    p_data[7] = p_frame->reserved_7;
    p_data[8] = p_frame->reserved_8;
    p_data[9] = p_frame->reserved_9;
    p_data[10] = p_frame->reserved_10;
    p_data[11] = p_frame->reserved_11;
    p_data[12] = p_frame->reserved_12;
    p_data[13] = p_frame->reserved_13;
    p_data[14] = p_frame->reserved_14;
    p_data[15] = ComLighting_CalculateChecksum(p_data);
}

uint8_t ComLighting_DecodeBcmCommand(const uint8_t * p_data,
                                     Lighting_BcmCommandFrame_t * p_frame,
                                     uint8_t * p_rx_checksum,
                                     uint8_t * p_calc_checksum)
{
    uint8_t valid_frame;

    valid_frame = 0U;

    if ((p_data != NULL) && (p_frame != NULL) &&
        (p_rx_checksum != NULL) && (p_calc_checksum != NULL))
    {
        *p_rx_checksum = p_data[15];
        *p_calc_checksum = ComLighting_CalculateChecksum(p_data);

        if (*p_rx_checksum == *p_calc_checksum)
        {
            p_frame->command_bits = p_data[0];
            p_frame->heartbeat_counter = p_data[1];
            p_frame->sender_node_id = p_data[2];
            p_frame->bcm_alive = p_data[3];
            p_frame->rolling_counter = p_data[4];
            p_frame->reserved_5 = p_data[5];
            p_frame->reserved_6 = p_data[6];
            p_frame->reserved_7 = p_data[7];
            p_frame->reserved_8 = p_data[8];
            p_frame->reserved_9 = p_data[9];
            p_frame->reserved_10 = p_data[10];
            p_frame->reserved_11 = p_data[11];
            p_frame->reserved_12 = p_data[12];
            p_frame->reserved_13 = p_data[13];
            p_frame->reserved_14 = p_data[14];
            p_frame->checksum = p_data[15];
            valid_frame = 1U;
        }
    }

    return valid_frame;
}

void ComLighting_EncodeLightingStatus(const Lighting_StatusFrame_t * p_frame,
                                      uint8_t * p_data)
{
    if ((p_frame == NULL) || (p_data == NULL))
    {
        return;
    }

    p_data[0] = p_frame->flags;
    p_data[1] = p_frame->state;
    p_data[2] = p_frame->last_heartbeat_rx;
    p_data[3] = p_frame->status_counter;
    p_data[4] = p_frame->command_bits_echo;
    p_data[5] = p_frame->rx_count_lsb;
    p_data[6] = p_frame->heartbeat_led_state;
    p_data[7] = p_frame->indicator_blink_state;
    p_data[8] = p_frame->head_command_echo;
    p_data[9] = p_frame->park_command_echo;
    p_data[10] = p_frame->left_command_echo;
    p_data[11] = p_frame->right_command_echo;
    p_data[12] = p_frame->hazard_command_echo;
    p_data[13] = p_frame->diagnostic_pattern_a5;
    p_data[14] = p_frame->node_id;
    p_data[15] = ComLighting_CalculateChecksum(p_data);
}

uint8_t ComLighting_DecodeLightingStatus(const uint8_t * p_data,
                                         Lighting_StatusFrame_t * p_frame,
                                         uint8_t * p_rx_checksum,
                                         uint8_t * p_calc_checksum)
{
    uint8_t valid_frame;

    valid_frame = 0U;

    if ((p_data != NULL) && (p_frame != NULL) &&
        (p_rx_checksum != NULL) && (p_calc_checksum != NULL))
    {
        *p_rx_checksum = p_data[15];
        *p_calc_checksum = ComLighting_CalculateChecksum(p_data);

        if (*p_rx_checksum == *p_calc_checksum)
        {
            p_frame->flags = p_data[0];
            p_frame->state = p_data[1];
            p_frame->last_heartbeat_rx = p_data[2];
            p_frame->status_counter = p_data[3];
            p_frame->command_bits_echo = p_data[4];
            p_frame->rx_count_lsb = p_data[5];
            p_frame->heartbeat_led_state = p_data[6];
            p_frame->indicator_blink_state = p_data[7];
            p_frame->head_command_echo = p_data[8];
            p_frame->park_command_echo = p_data[9];
            p_frame->left_command_echo = p_data[10];
            p_frame->right_command_echo = p_data[11];
            p_frame->hazard_command_echo = p_data[12];
            p_frame->diagnostic_pattern_a5 = p_data[13];
            p_frame->node_id = p_data[14];
            p_frame->checksum = p_data[15];
            valid_frame = 1U;
        }
    }

    return valid_frame;
}

uint8_t ComLighting_IsFlagSet(uint8_t flags, uint8_t mask)
{
    uint8_t value;

    if ((flags & mask) != 0U)
    {
        value = 1U;
    }
    else
    {
        value = 0U;
    }

    return value;
}
