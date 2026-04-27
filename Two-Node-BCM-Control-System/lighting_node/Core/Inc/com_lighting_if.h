#ifndef COM_LIGHTING_IF_H
#define COM_LIGHTING_IF_H

#include <stdint.h>
#include "platform_types.h"
#include "lighting_messages.h"

/*
 * CRC-8/SAE-J1850: poly=0x1D, init=0xFF, final_xor=0xFF, no reflection.
 * Replaces the former XOR checksum to satisfy ISO 26262 diagnostic coverage
 * requirements; CRC-8 detects all single-bit errors, all odd-bit errors, and
 * all burst errors of length <= 8, unlike simple XOR which misses many
 * multi-bit and byte-swap patterns.
 */
uint8_t ComLighting_CalculateCRC8(const uint8_t * p_data);

void    ComLighting_EncodeBcmCommand(const Lighting_BcmCommandFrame_t * p_frame,
                                     uint8_t * p_data);
bool_t  ComLighting_DecodeBcmCommand(const uint8_t * p_data,
                                     Lighting_BcmCommandFrame_t * p_frame,
                                     uint8_t * p_rx_checksum,
                                     uint8_t * p_calc_checksum);

void    ComLighting_EncodeLightingStatus(const Lighting_StatusFrame_t * p_frame,
                                         uint8_t * p_data);
bool_t  ComLighting_DecodeLightingStatus(const uint8_t * p_data,
                                         Lighting_StatusFrame_t * p_frame,
                                         uint8_t * p_rx_checksum,
                                         uint8_t * p_calc_checksum);

bool_t  ComLighting_IsFlagSet(uint8_t flags, uint8_t mask);

#endif /* COM_LIGHTING_IF_H */
