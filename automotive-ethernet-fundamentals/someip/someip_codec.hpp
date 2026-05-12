#pragma once

#include <cstdint>
#include <cstddef>
#include "../common/byte_order.hpp"

// SOME/IP header is exactly 16 bytes, all multi-byte fields big-endian.
constexpr size_t SOMEIP_HEADER_SIZE = 16;

// Message Type values (offset 14 in the header).
constexpr uint8_t SOMEIP_MSG_TYPE_REQUEST  = 0x00;
constexpr uint8_t SOMEIP_MSG_TYPE_RESPONSE = 0x80;
constexpr uint8_t SOMEIP_MSG_TYPE_ERROR    = 0x81;

// Return Code values (offset 15 in the header).
constexpr uint8_t SOMEIP_RETURN_CODE_E_OK = 0x00;

// Structured representation of a SOME/IP header.
struct SomeIpHeader {
    uint16_t service_id;
    uint16_t method_id;
    uint32_t length;
    uint16_t client_id;
    uint16_t session_id;
    uint8_t  protocol_version;
    uint8_t  interface_version;
    uint8_t  message_type;
    uint8_t  return_code;
};

// Parse 16 bytes at the start of buf into a SomeIpHeader.
// Returns true on success; false if buf is smaller than SOMEIP_HEADER_SIZE.
inline bool parse_someip_header(const uint8_t* buf, size_t len, SomeIpHeader& out) {
    if (len < SOMEIP_HEADER_SIZE) {
        return false;
    }
    out.service_id        = read_u16_be(buf, 0);
    out.method_id         = read_u16_be(buf, 2);
    out.length            = read_u32_be(buf, 4);
    out.client_id         = read_u16_be(buf, 8);
    out.session_id        = read_u16_be(buf, 10);
    out.protocol_version  = buf[12];
    out.interface_version = buf[13];
    out.message_type      = buf[14];
    out.return_code       = buf[15];
    return true;
}

// Serialize a SomeIpHeader into 16 bytes at the start of buf.
// Caller is responsible for ensuring buf has at least SOMEIP_HEADER_SIZE bytes.
inline void write_someip_header(uint8_t* buf, const SomeIpHeader& hdr) {
    write_u16_be(buf, 0, hdr.service_id);
    write_u16_be(buf, 2, hdr.method_id);
    write_u32_be(buf, 4, hdr.length);
    write_u16_be(buf, 8, hdr.client_id);
    write_u16_be(buf, 10, hdr.session_id);
    buf[12] = hdr.protocol_version;
    buf[13] = hdr.interface_version;
    buf[14] = hdr.message_type;
    buf[15] = hdr.return_code;
}
