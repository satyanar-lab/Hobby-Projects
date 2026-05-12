#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include "../common/byte_order.hpp"

// ============================================================
// UDS protocol constants
// ============================================================

// Positive response SID = request SID + 0x40 (ISO 14229 convention).
constexpr uint8_t UDS_POSITIVE_RESPONSE_OFFSET = 0x40;

// Service IDs (a real ECU supports many; we only use one here).
constexpr uint8_t UDS_SID_READ_DATA_BY_IDENTIFIER     = 0x22;
constexpr uint8_t UDS_SID_READ_DATA_BY_IDENTIFIER_POS = 0x62;

// First byte of any negative response.
constexpr uint8_t UDS_NEGATIVE_RESPONSE = 0x7F;

// Negative Response Codes (NRCs) — only the ones our server emits.
constexpr uint8_t NRC_SERVICE_NOT_SUPPORTED      = 0x11;
constexpr uint8_t NRC_INCORRECT_MESSAGE_LENGTH   = 0x13;
constexpr uint8_t NRC_REQUEST_OUT_OF_RANGE       = 0x31;

// Data Identifiers (DIDs) — only the one our server supports.
constexpr uint16_t UDS_DID_VIN = 0xF190;

// Minimum sizes for various UDS messages.
constexpr size_t UDS_READ_DID_REQUEST_SIZE      = 3;  // SID + DID
constexpr size_t UDS_READ_DID_POS_RESPONSE_MIN  = 3;  // 0x62 + DID, plus data
constexpr size_t UDS_NEGATIVE_RESPONSE_SIZE     = 3;  // 0x7F + SID + NRC

// ============================================================
// Structured representations
// ============================================================

// A ReadDataByIdentifier request: 3 bytes on the wire.
struct UdsReadDidRequest {
    uint8_t  sid;   // 0x22 for valid requests
    uint16_t did;   // big-endian on the wire
};

// A negative response: 3 bytes on the wire.
struct UdsNegativeResponse {
    uint8_t response_sid;   // always 0x7F
    uint8_t rejected_sid;   // echo of the original request SID
    uint8_t nrc;            // negative response code
};

// ============================================================
// Request parsing
// ============================================================

// Parse a 3-byte UDS ReadDataByIdentifier request from buf.
// Returns true on success; false if buf is shorter than 3 bytes.
inline bool parse_uds_read_did_request(const uint8_t* buf,
                                       size_t len,
                                       UdsReadDidRequest& out) {
    if (len < UDS_READ_DID_REQUEST_SIZE) {
        return false;
    }
    out.sid = buf[0];
    out.did = read_u16_be(buf, 1);
    return true;
}

// ============================================================
// Response building
// ============================================================

// Build a positive response (0x62 [DID] [data...]) into buf.
// Returns the total response size; assumes buf is large enough.
inline size_t build_uds_positive_response(uint8_t* buf,
                                          uint16_t did,
                                          const uint8_t* data,
                                          size_t data_len) {
    buf[0] = UDS_SID_READ_DATA_BY_IDENTIFIER_POS;
    write_u16_be(buf, 1, did);
    memcpy(buf + 3, data, data_len);
    return 3 + data_len;
}

// Build a negative response (0x7F [rejected SID] [NRC]) into buf.
// Returns the total response size (always 3); assumes buf has room.
inline size_t build_uds_negative_response(uint8_t* buf,
                                          uint8_t rejected_sid,
                                          uint8_t nrc) {
    buf[0] = UDS_NEGATIVE_RESPONSE;
    buf[1] = rejected_sid;
    buf[2] = nrc;
    return UDS_NEGATIVE_RESPONSE_SIZE;
}

// ============================================================
// Response parsing (client-side)
// ============================================================

// Classifies a UDS response by its first byte.
enum class UdsResponseKind {
    Positive,    // first byte is a positive-response SID (e.g. 0x62)
    Negative,    // first byte is 0x7F (followed by SID + NRC)
    Unexpected,  // anything else
    TooShort     // fewer than 1 byte
};

// Determine the kind of UDS response in buf.
inline UdsResponseKind classify_uds_response(const uint8_t* buf, size_t len) {
    if (len < 1) return UdsResponseKind::TooShort;
    if (buf[0] == UDS_SID_READ_DATA_BY_IDENTIFIER_POS) return UdsResponseKind::Positive;
    if (buf[0] == UDS_NEGATIVE_RESPONSE && len >= 3)   return UdsResponseKind::Negative;
    return UdsResponseKind::Unexpected;
}

// Parse a negative response (3 bytes: 0x7F + SID + NRC).
// Returns true on success; false if buf is too short or wrong shape.
inline bool parse_uds_negative_response(const uint8_t* buf,
                                        size_t len,
                                        UdsNegativeResponse& out) {
    if (len < UDS_NEGATIVE_RESPONSE_SIZE || buf[0] != UDS_NEGATIVE_RESPONSE) {
        return false;
    }
    out.response_sid = buf[0];
    out.rejected_sid = buf[1];
    out.nrc          = buf[2];
    return true;
}
