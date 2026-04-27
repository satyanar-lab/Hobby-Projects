/*
 * test_checksum_validation.c
 *
 * Unit tests for ComLighting_CalculateChecksum,
 * ComLighting_EncodeBcmCommand, ComLighting_DecodeBcmCommand,
 * ComLighting_EncodeLightingStatus, and ComLighting_DecodeLightingStatus.
 *
 * These functions have no HAL dependencies and link cleanly on the host.
 */

#include <stdint.h>
#include <string.h>
#include "lighting_messages.h"
#include "com_lighting_if.h"

extern void test_assert(int condition, const char *msg);

/* ---- Tests --------------------------------------------------------------- */

void test_checksum_validation_run(void)
{
    uint8_t buf[LIGHTING_FRAME_LENGTH];
    uint8_t rx_cs, calc_cs;
    uint8_t valid;
    Lighting_BcmCommandFrame_t cmd_frame;
    Lighting_BcmCommandFrame_t decoded_cmd;
    Lighting_StatusFrame_t     status_frame;
    Lighting_StatusFrame_t     decoded_status;

    /* T1: zero buffer — XOR of 15 zeros is zero */
    memset(buf, 0, sizeof(buf));
    test_assert(ComLighting_CalculateChecksum(buf) == 0x00U,
                "T1: checksum of all-zero payload is 0x00");

    /* T2: NULL pointer safety */
    test_assert(ComLighting_CalculateChecksum(NULL) == 0x00U,
                "T2: NULL pointer returns 0 without crash");

    /* T3: known-value checksum — bytes 0x11..0x1F XOR */
    {
        uint8_t i;
        uint8_t expected = 0U;
        for (i = 0U; i < LIGHTING_FRAME_CHECKSUM_INDEX; i++)
        {
            buf[i] = (uint8_t)(0x11U + i);
            expected ^= buf[i];
        }
        test_assert(ComLighting_CalculateChecksum(buf) == expected,
                    "T3: sequential payload checksum matches manual XOR");
    }

    /* T4: encode BCM command and verify byte[15] == XOR[0..14] */
    memset(&cmd_frame, 0, sizeof(cmd_frame));
    cmd_frame.command_bits      = LIGHTING_CMD_HEAD_LAMP_BIT | LIGHTING_CMD_PARK_LAMP_BIT;
    cmd_frame.heartbeat_counter = 42U;
    cmd_frame.sender_node_id    = 1U;
    cmd_frame.bcm_alive         = 1U;
    cmd_frame.rolling_counter   = 7U;
    cmd_frame.reserved_5        = 0x11U;
    cmd_frame.reserved_6        = 0x22U;
    cmd_frame.reserved_7        = 0x33U;
    cmd_frame.reserved_8        = 0x44U;
    cmd_frame.reserved_9        = 0x55U;
    cmd_frame.reserved_10       = 0x66U;
    cmd_frame.reserved_11       = 0x77U;
    cmd_frame.reserved_12       = 0x88U;
    cmd_frame.reserved_13       = 0x99U;
    cmd_frame.reserved_14       = 0xAAU;

    ComLighting_EncodeBcmCommand(&cmd_frame, buf);
    calc_cs = ComLighting_CalculateChecksum(buf);
    test_assert(buf[15] == calc_cs, "T4: encoded BCM command checksum byte matches calculated");

    /* T5: decode with valid checksum succeeds */
    memset(&decoded_cmd, 0, sizeof(decoded_cmd));
    valid = ComLighting_DecodeBcmCommand(buf, &decoded_cmd, &rx_cs, &calc_cs);
    test_assert(valid == 1U, "T5: decode valid BCM command returns valid=1");
    test_assert(decoded_cmd.command_bits == cmd_frame.command_bits,
                "T5: decoded command_bits matches original");
    test_assert(decoded_cmd.heartbeat_counter == cmd_frame.heartbeat_counter,
                "T5: decoded heartbeat_counter matches original");

    /* T6: single-bit corruption in data causes decode failure */
    buf[0] ^= 0x01U;
    valid = ComLighting_DecodeBcmCommand(buf, &decoded_cmd, &rx_cs, &calc_cs);
    test_assert(valid == 0U, "T6: corrupted data byte causes decode to return valid=0");
    test_assert(rx_cs != calc_cs, "T6: rx_checksum and calc_checksum differ on corruption");

    /* T7: single-bit corruption in checksum byte causes decode failure */
    buf[0] ^= 0x01U;  /* restore data */
    buf[15] ^= 0x80U; /* corrupt checksum */
    valid = ComLighting_DecodeBcmCommand(buf, &decoded_cmd, &rx_cs, &calc_cs);
    test_assert(valid == 0U, "T7: corrupted checksum byte causes decode to return valid=0");

    /* T8: encode + decode round-trip for Lighting Status frame */
    memset(&status_frame, 0, sizeof(status_frame));
    status_frame.flags                = LIGHTING_STATUS_HEAD_ACTIVE_BIT | LIGHTING_STATUS_PARK_ACTIVE_BIT;
    status_frame.state                = LIGHTING_STATE_HEAD_ACTIVE;
    status_frame.last_heartbeat_rx    = 42U;
    status_frame.status_counter       = 5U;
    status_frame.command_bits_echo    = LIGHTING_CMD_HEAD_LAMP_BIT;
    status_frame.diagnostic_pattern_a5 = 0xA5U;
    status_frame.node_id              = 2U;

    ComLighting_EncodeLightingStatus(&status_frame, buf);
    memset(&decoded_status, 0, sizeof(decoded_status));
    valid = ComLighting_DecodeLightingStatus(buf, &decoded_status, &rx_cs, &calc_cs);
    test_assert(valid == 1U, "T8: lighting status round-trip valid=1");
    test_assert(decoded_status.state == LIGHTING_STATE_HEAD_ACTIVE,
                "T8: decoded state matches original");
    test_assert(decoded_status.diagnostic_pattern_a5 == 0xA5U,
                "T8: diagnostic pattern 0xA5 preserved through encode/decode");

    /* T9: NULL frame pointer on encode is safe (no crash) */
    ComLighting_EncodeBcmCommand(NULL, buf);  /* should return without writing */
    ComLighting_EncodeBcmCommand(&cmd_frame, NULL);
    test_assert(1, "T9: NULL pointer encode calls do not crash");

    /* T10: NULL pointer decode returns invalid */
    valid = ComLighting_DecodeBcmCommand(NULL, &decoded_cmd, &rx_cs, &calc_cs);
    test_assert(valid == 0U, "T10: NULL data pointer decode returns valid=0");
    valid = ComLighting_DecodeBcmCommand(buf, NULL, &rx_cs, &calc_cs);
    test_assert(valid == 0U, "T10: NULL frame pointer decode returns valid=0");
}
