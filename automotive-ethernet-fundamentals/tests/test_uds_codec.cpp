#include <gtest/gtest.h>
#include "../uds/uds_codec.hpp"

// ============================================================
// parse_uds_read_did_request — parse 3-byte ReadDID request
// ============================================================

// Basic: parse a well-formed Read VIN request.
TEST(UdsCodec, ParseValidRequest) {
    uint8_t bytes[] = {0x22, 0xF1, 0x90};
    UdsReadDidRequest req;
    ASSERT_TRUE(parse_uds_read_did_request(bytes, 3, req));
    EXPECT_EQ(req.sid, UDS_SID_READ_DATA_BY_IDENTIFIER);
    EXPECT_EQ(req.did, UDS_DID_VIN);
}

// Parsing must reject buffers shorter than 3 bytes.
TEST(UdsCodec, ParseTooShort) {
    uint8_t bytes[] = {0x22, 0xF1};
    UdsReadDidRequest req;
    EXPECT_FALSE(parse_uds_read_did_request(bytes, 2, req));
}

// Empty buffer must be rejected without crashing.
TEST(UdsCodec, ParseEmpty) {
    uint8_t bytes[1] = {};
    UdsReadDidRequest req;
    EXPECT_FALSE(parse_uds_read_did_request(bytes, 0, req));
}

// ============================================================
// build_uds_positive_response — 0x62 [DID] [data...]
// ============================================================

// Basic: build a positive response with a 17-byte VIN payload.
TEST(UdsCodec, BuildPositiveResponseVin) {
    const char vin[] = "VIN1234567890ABCD";
    uint8_t buf[64] = {};
    size_t size = build_uds_positive_response(
        buf, UDS_DID_VIN,
        reinterpret_cast<const uint8_t*>(vin), 17);

    EXPECT_EQ(size, 20u);                 // 3 header + 17 data
    EXPECT_EQ(buf[0], 0x62);              // positive response SID
    EXPECT_EQ(buf[1], 0xF1);              // DID high byte
    EXPECT_EQ(buf[2], 0x90);              // DID low byte
    EXPECT_EQ(buf[3], 'V');               // first VIN byte
    EXPECT_EQ(buf[19], 'D');              // last VIN byte
}

// Positive response with empty data payload — should be exactly 3 bytes.
TEST(UdsCodec, BuildPositiveResponseEmptyData) {
    uint8_t buf[16] = {};
    size_t size = build_uds_positive_response(buf, 0x1234, nullptr, 0);
    EXPECT_EQ(size, 3u);
    EXPECT_EQ(buf[0], 0x62);
    EXPECT_EQ(buf[1], 0x12);
    EXPECT_EQ(buf[2], 0x34);
}

// ============================================================
// build_uds_negative_response — 0x7F [SID] [NRC]
// ============================================================

// Basic: build a negative response with NRC 0x31.
TEST(UdsCodec, BuildNegativeResponseOutOfRange) {
    uint8_t buf[8] = {};
    size_t size = build_uds_negative_response(
        buf, UDS_SID_READ_DATA_BY_IDENTIFIER, NRC_REQUEST_OUT_OF_RANGE);

    EXPECT_EQ(size, 3u);
    EXPECT_EQ(buf[0], 0x7F);
    EXPECT_EQ(buf[1], 0x22);
    EXPECT_EQ(buf[2], 0x31);
}

// Negative response with NRC 0x11 (service not supported).
TEST(UdsCodec, BuildNegativeResponseServiceNotSupported) {
    uint8_t buf[8] = {};
    size_t size = build_uds_negative_response(
        buf, 0x10, NRC_SERVICE_NOT_SUPPORTED);

    EXPECT_EQ(size, 3u);
    EXPECT_EQ(buf[0], 0x7F);
    EXPECT_EQ(buf[1], 0x10);
    EXPECT_EQ(buf[2], 0x11);
}

// ============================================================
// classify_uds_response — determine response kind from bytes
// ============================================================

// First byte 0x62 → Positive.
TEST(UdsCodec, ClassifyPositive) {
    uint8_t bytes[] = {0x62, 0xF1, 0x90};
    EXPECT_EQ(classify_uds_response(bytes, 3), UdsResponseKind::Positive);
}

// First byte 0x7F with 3+ bytes → Negative.
TEST(UdsCodec, ClassifyNegative) {
    uint8_t bytes[] = {0x7F, 0x22, 0x31};
    EXPECT_EQ(classify_uds_response(bytes, 3), UdsResponseKind::Negative);
}

// First byte 0x7F but fewer than 3 bytes → Unexpected (malformed negative).
TEST(UdsCodec, ClassifyMalformedNegative) {
    uint8_t bytes[] = {0x7F, 0x22};
    EXPECT_EQ(classify_uds_response(bytes, 2), UdsResponseKind::Unexpected);
}

// Unknown first byte → Unexpected.
TEST(UdsCodec, ClassifyUnexpected) {
    uint8_t bytes[] = {0xAB, 0xCD};
    EXPECT_EQ(classify_uds_response(bytes, 2), UdsResponseKind::Unexpected);
}

// Empty buffer → TooShort.
TEST(UdsCodec, ClassifyTooShort) {
    uint8_t bytes[1] = {};
    EXPECT_EQ(classify_uds_response(bytes, 0), UdsResponseKind::TooShort);
}

// ============================================================
// parse_uds_negative_response — 3-byte negative response parse
// ============================================================

// Basic: parse a 0x7F 0x22 0x31 negative response.
TEST(UdsCodec, ParseNegativeResponse) {
    uint8_t bytes[] = {0x7F, 0x22, 0x31};
    UdsNegativeResponse neg;
    ASSERT_TRUE(parse_uds_negative_response(bytes, 3, neg));
    EXPECT_EQ(neg.response_sid, 0x7F);
    EXPECT_EQ(neg.rejected_sid, 0x22);
    EXPECT_EQ(neg.nrc, 0x31);
}

// Reject if buffer is shorter than 3 bytes.
TEST(UdsCodec, ParseNegativeResponseTooShort) {
    uint8_t bytes[] = {0x7F, 0x22};
    UdsNegativeResponse neg;
    EXPECT_FALSE(parse_uds_negative_response(bytes, 2, neg));
}

// Reject if first byte is not 0x7F.
TEST(UdsCodec, ParseNegativeResponseWrongPrefix) {
    uint8_t bytes[] = {0x62, 0x22, 0x31};
    UdsNegativeResponse neg;
    EXPECT_FALSE(parse_uds_negative_response(bytes, 3, neg));
}
