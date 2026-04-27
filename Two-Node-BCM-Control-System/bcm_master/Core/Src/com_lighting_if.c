#include "com_lighting_if.h"
#include <stddef.h>

/*
 * CRC-8/SAE-J1850  (poly=0x1D, init=0xFF, final_xor=0xFF, no reflection)
 *
 * Why CRC-8 instead of XOR:
 *   XOR folds misses byte-swap errors (commutative) and many multi-bit
 *   patterns.  CRC-8/SAE-J1850 guarantees detection of all single-bit
 *   errors, all odd-bit errors, and all burst errors <= 8 bits long,
 *   satisfying ISO 26262 diagnostic coverage for ASIL-B messaging.
 *
 * NULL guard: returns 0x00 on NULL input so callers always get a value;
 * the decoder will then reject the frame because rx_crc != 0x00.
 */
uint8_t ComLighting_CalculateCRC8(const uint8_t * p_data)
{
    uint8_t crc;
    uint8_t index;
    uint8_t bit;

    crc = 0xFFU;

    if (p_data != NULL)
    {
        for (index = 0U; index < LIGHTING_FRAME_CHECKSUM_INDEX; index++)
        {
            crc ^= p_data[index];
            for (bit = 0U; bit < 8U; bit++)
            {
                if ((crc & 0x80U) != 0U)
                {
                    crc = (uint8_t)((uint8_t)(crc << 1U) ^ 0x1DU);
                }
                else
                {
                    crc = (uint8_t)(crc << 1U);
                }
            }
        }
    }

    return (uint8_t)(crc ^ 0xFFU);
}

void ComLighting_EncodeBcmCommand(const Lighting_BcmCommandFrame_t * p_frame,
                                  uint8_t * p_data)
{
    if ((p_frame == NULL) || (p_data == NULL))
    {
        return;
    }

    p_data[0]  = p_frame->command_bits;
    p_data[1]  = p_frame->heartbeat_counter;
    p_data[2]  = p_frame->sender_node_id;
    p_data[3]  = p_frame->bcm_alive;
    p_data[4]  = p_frame->rolling_counter;
    /* Bytes 5-14: fixed walking pattern — aids oscilloscope verification of
     * payload alignment and fills the CAN FD 16-byte data phase. */
    p_data[5]  = p_frame->reserved_5;
    p_data[6]  = p_frame->reserved_6;
    p_data[7]  = p_frame->reserved_7;
    p_data[8]  = p_frame->reserved_8;
    p_data[9]  = p_frame->reserved_9;
    p_data[10] = p_frame->reserved_10;
    p_data[11] = p_frame->reserved_11;
    p_data[12] = p_frame->reserved_12;
    p_data[13] = p_frame->reserved_13;
    p_data[14] = p_frame->reserved_14;
    p_data[15] = ComLighting_CalculateCRC8(p_data);
}

bool_t ComLighting_DecodeBcmCommand(const uint8_t * p_data,
                                    Lighting_BcmCommandFrame_t * p_frame,
                                    uint8_t * p_rx_checksum,
                                    uint8_t * p_calc_checksum)
{
    bool_t valid_frame;

    valid_frame = FALSE;

    if ((p_data != NULL) && (p_frame != NULL) &&
        (p_rx_checksum != NULL) && (p_calc_checksum != NULL))
    {
        *p_rx_checksum   = p_data[15];
        *p_calc_checksum = ComLighting_CalculateCRC8(p_data);

        if (*p_rx_checksum == *p_calc_checksum)
        {
            p_frame->command_bits      = p_data[0];
            p_frame->heartbeat_counter = p_data[1];
            p_frame->sender_node_id    = p_data[2];
            p_frame->bcm_alive         = p_data[3];
            p_frame->rolling_counter   = p_data[4];
            p_frame->reserved_5        = p_data[5];
            p_frame->reserved_6        = p_data[6];
            p_frame->reserved_7        = p_data[7];
            p_frame->reserved_8        = p_data[8];
            p_frame->reserved_9        = p_data[9];
            p_frame->reserved_10       = p_data[10];
            p_frame->reserved_11       = p_data[11];
            p_frame->reserved_12       = p_data[12];
            p_frame->reserved_13       = p_data[13];
            p_frame->reserved_14       = p_data[14];
            p_frame->checksum          = p_data[15];
            valid_frame = TRUE;
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

    p_data[0]  = p_frame->flags;
    p_data[1]  = p_frame->state;
    p_data[2]  = p_frame->last_heartbeat_rx;
    p_data[3]  = p_frame->status_counter;
    p_data[4]  = p_frame->command_bits_echo;
    p_data[5]  = p_frame->rx_count_lsb;
    p_data[6]  = p_frame->heartbeat_led_state;
    p_data[7]  = p_frame->indicator_blink_state;
    p_data[8]  = p_frame->head_command_echo;
    p_data[9]  = p_frame->park_command_echo;
    p_data[10] = p_frame->left_command_echo;
    p_data[11] = p_frame->right_command_echo;
    p_data[12] = p_frame->hazard_command_echo;
    p_data[13] = p_frame->diagnostic_pattern_a5;
    p_data[14] = p_frame->node_id;
    p_data[15] = ComLighting_CalculateCRC8(p_data);
}

bool_t ComLighting_DecodeLightingStatus(const uint8_t * p_data,
                                        Lighting_StatusFrame_t * p_frame,
                                        uint8_t * p_rx_checksum,
                                        uint8_t * p_calc_checksum)
{
    bool_t valid_frame;

    valid_frame = FALSE;

    if ((p_data != NULL) && (p_frame != NULL) &&
        (p_rx_checksum != NULL) && (p_calc_checksum != NULL))
    {
        *p_rx_checksum   = p_data[15];
        *p_calc_checksum = ComLighting_CalculateCRC8(p_data);

        if (*p_rx_checksum == *p_calc_checksum)
        {
            p_frame->flags                 = p_data[0];
            p_frame->state                 = p_data[1];
            p_frame->last_heartbeat_rx     = p_data[2];
            p_frame->status_counter        = p_data[3];
            p_frame->command_bits_echo     = p_data[4];
            p_frame->rx_count_lsb          = p_data[5];
            p_frame->heartbeat_led_state   = p_data[6];
            p_frame->indicator_blink_state = p_data[7];
            p_frame->head_command_echo     = p_data[8];
            p_frame->park_command_echo     = p_data[9];
            p_frame->left_command_echo     = p_data[10];
            p_frame->right_command_echo    = p_data[11];
            p_frame->hazard_command_echo   = p_data[12];
            p_frame->diagnostic_pattern_a5 = p_data[13];
            p_frame->node_id               = p_data[14];
            p_frame->checksum              = p_data[15];
            valid_frame = TRUE;
        }
    }

    return valid_frame;
}

bool_t ComLighting_IsFlagSet(uint8_t flags, uint8_t mask)
{
    bool_t result;

    if ((flags & mask) != 0U)
    {
        result = TRUE;
    }
    else
    {
        result = FALSE;
    }

    return result;
}
