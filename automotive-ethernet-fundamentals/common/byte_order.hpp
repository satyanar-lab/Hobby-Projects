#pragma once

#include <cstdint>
#include <cstddef>

// Read a 16-bit big-endian value from buffer at offset
inline uint16_t read_u16_be(const uint8_t* buf, size_t offset) {
    return (static_cast<uint16_t>(buf[offset]) << 8)
         |  static_cast<uint16_t>(buf[offset + 1]);
}

// Read a 32-bit big-endian value from buffer at offset
inline uint32_t read_u32_be(const uint8_t* buf, size_t offset) {
    return (static_cast<uint32_t>(buf[offset])     << 24)
         | (static_cast<uint32_t>(buf[offset + 1]) << 16)
         | (static_cast<uint32_t>(buf[offset + 2]) << 8)
         |  static_cast<uint32_t>(buf[offset + 3]);
}

// Write a 16-bit value to buffer at offset, big-endian
inline void write_u16_be(uint8_t* buf, size_t offset, uint16_t value) {
    buf[offset]     = (value >> 8) & 0xFF;
    buf[offset + 1] =  value       & 0xFF;
}

// Write a 32-bit value to buffer at offset, big-endian
inline void write_u32_be(uint8_t* buf, size_t offset, uint32_t value) {
    buf[offset]     = (value >> 24) & 0xFF;
    buf[offset + 1] = (value >> 16) & 0xFF;
    buf[offset + 2] = (value >>  8) & 0xFF;
    buf[offset + 3] =  value        & 0xFF;
}
