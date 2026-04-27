#ifndef COM_LIGHTING_IF_H
#define COM_LIGHTING_IF_H

#include <stdint.h>
#include "lighting_messages.h"

uint8_t ComLighting_CalculateChecksum(const uint8_t * p_data);
void ComLighting_EncodeBcmCommand(const Lighting_BcmCommandFrame_t * p_frame,
                                  uint8_t * p_data);
uint8_t ComLighting_DecodeBcmCommand(const uint8_t * p_data,
                                     Lighting_BcmCommandFrame_t * p_frame,
                                     uint8_t * p_rx_checksum,
                                     uint8_t * p_calc_checksum);
void ComLighting_EncodeLightingStatus(const Lighting_StatusFrame_t * p_frame,
                                      uint8_t * p_data);
uint8_t ComLighting_DecodeLightingStatus(const uint8_t * p_data,
                                         Lighting_StatusFrame_t * p_frame,
                                         uint8_t * p_rx_checksum,
                                         uint8_t * p_calc_checksum);
uint8_t ComLighting_IsFlagSet(uint8_t flags, uint8_t mask);

#endif /* COM_LIGHTING_IF_H */
