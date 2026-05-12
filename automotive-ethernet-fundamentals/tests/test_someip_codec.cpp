#include <gtest/gtest.h>
#include "../someip/someip_codec.hpp"

// ============================================================
// parse_someip_header — parse 16-byte header from raw bytes
// ============================================================

// Basic: parse a well-formed SOME/IP REQUEST header.
TEST(SomeIpCodec, ParseValidRequest) {
    uint8_t bytes[16] = {
        0x51, 0x00,             // Service ID 0x5100
        0x00, 0x01,             // Method ID 0x0001
        0x00, 0x00, 0x00, 0x0C, // Length 12
        0x00, 0x01,             // Client ID 0x0001
        0x00, 0x01,             // Session ID 0x0001
        0x01,                   // Protocol Version
        0x01,                   // Interface Version
        0x00,                   // Message Type (REQUEST)
        0x00                    // Return Code (E_OK)
    };

    SomeIpHeader hdr;
    ASSERT_TRUE(parse_someip_header(bytes, 16, hdr));
    EXPECT_EQ(hdr.service_id, 0x5100);
    EXPECT_EQ(hdr.method_id, 0x0001);
    EXPECT_EQ(hdr.length, 12u);
    EXPECT_EQ(hdr.client_id, 0x0001);
    EXPECT_EQ(hdr.session_id, 0x0001);
    EXPECT_EQ(hdr.protocol_version, 0x01);
    EXPECT_EQ(hdr.interface_version, 0x01);
    EXPECT_EQ(hdr.message_type, SOMEIP_MSG_TYPE_REQUEST);
    EXPECT_EQ(hdr.return_code, SOMEIP_RETURN_CODE_E_OK);
}

// Parsing must reject buffers shorter than 16 bytes.
TEST(SomeIpCodec, ParseTooShort) {
    uint8_t bytes[15] = {};
    SomeIpHeader hdr;
    EXPECT_FALSE(parse_someip_header(bytes, 15, hdr));
}

// Empty buffer must also be rejected without crashing.
TEST(SomeIpCodec, ParseEmpty) {
    uint8_t bytes[1] = {};
    SomeIpHeader hdr;
    EXPECT_FALSE(parse_someip_header(bytes, 0, hdr));
}

// Exactly 16 bytes is the minimum valid length.
TEST(SomeIpCodec, ParseExactlyHeaderSize) {
    uint8_t bytes[16] = {};
    SomeIpHeader hdr;
    EXPECT_TRUE(parse_someip_header(bytes, 16, hdr));
}

// Message type 0x80 should be recognized as RESPONSE.
TEST(SomeIpCodec, ParseResponseMessageType) {
    uint8_t bytes[16] = {};
    bytes[14] = 0x80;  // Message Type = RESPONSE
    SomeIpHeader hdr;
    ASSERT_TRUE(parse_someip_header(bytes, 16, hdr));
    EXPECT_EQ(hdr.message_type, SOMEIP_MSG_TYPE_RESPONSE);
}

// ============================================================
// write_someip_header — serialize header to raw bytes
// ============================================================

// Basic: serialize a known header and verify each byte.
TEST(SomeIpCodec, WriteValidRequest) {
    SomeIpHeader hdr{};
    hdr.service_id        = 0x5100;
    hdr.method_id         = 0x0001;
    hdr.length            = 12;
    hdr.client_id         = 0x0001;
    hdr.session_id        = 0x0001;
    hdr.protocol_version  = 0x01;
    hdr.interface_version = 0x01;
    hdr.message_type      = SOMEIP_MSG_TYPE_REQUEST;
    hdr.return_code       = SOMEIP_RETURN_CODE_E_OK;

    uint8_t buf[16] = {};
    write_someip_header(buf, hdr);

    EXPECT_EQ(buf[0], 0x51);  EXPECT_EQ(buf[1], 0x00);  // Service ID
    EXPECT_EQ(buf[2], 0x00);  EXPECT_EQ(buf[3], 0x01);  // Method ID
    EXPECT_EQ(buf[4], 0x00);  EXPECT_EQ(buf[5], 0x00);  // Length high half
    EXPECT_EQ(buf[6], 0x00);  EXPECT_EQ(buf[7], 0x0C);  // Length low half
    EXPECT_EQ(buf[8], 0x00);  EXPECT_EQ(buf[9], 0x01);  // Client ID
    EXPECT_EQ(buf[10], 0x00); EXPECT_EQ(buf[11], 0x01); // Session ID
    EXPECT_EQ(buf[12], 0x01);                            // Protocol Version
    EXPECT_EQ(buf[13], 0x01);                            // Interface Version
    EXPECT_EQ(buf[14], 0x00);                            // Message Type
    EXPECT_EQ(buf[15], 0x00);                            // Return Code
}

// ============================================================
// Round-trip — write then parse must recover the original
// ============================================================

// Full round-trip with a populated REQUEST header.
TEST(SomeIpCodec, RoundTripRequest) {
    SomeIpHeader original{};
    original.service_id        = 0x5100;
    original.method_id         = 0x0001;
    original.length            = 12;
    original.client_id         = 0x0001;
    original.session_id        = 0x0001;
    original.protocol_version  = 0x01;
    original.interface_version = 0x01;
    original.message_type      = SOMEIP_MSG_TYPE_REQUEST;
    original.return_code       = SOMEIP_RETURN_CODE_E_OK;

    uint8_t buf[16] = {};
    write_someip_header(buf, original);

    SomeIpHeader parsed;
    ASSERT_TRUE(parse_someip_header(buf, 16, parsed));
    EXPECT_EQ(parsed.service_id, original.service_id);
    EXPECT_EQ(parsed.method_id, original.method_id);
    EXPECT_EQ(parsed.length, original.length);
    EXPECT_EQ(parsed.client_id, original.client_id);
    EXPECT_EQ(parsed.session_id, original.session_id);
    EXPECT_EQ(parsed.protocol_version, original.protocol_version);
    EXPECT_EQ(parsed.interface_version, original.interface_version);
    EXPECT_EQ(parsed.message_type, original.message_type);
    EXPECT_EQ(parsed.return_code, original.return_code);
}

// Round-trip with a RESPONSE header (Message Type 0x80) and large fields.
TEST(SomeIpCodec, RoundTripResponseMaxValues) {
    SomeIpHeader original{};
    original.service_id        = 0xFFFF;
    original.method_id         = 0xFFFF;
    original.length            = 0xFFFFFFFFu;
    original.client_id         = 0xFFFF;
    original.session_id        = 0xFFFF;
    original.protocol_version  = 0xFF;
    original.interface_version = 0xFF;
    original.message_type      = SOMEIP_MSG_TYPE_RESPONSE;
    original.return_code       = 0xFF;

    uint8_t buf[16] = {};
    write_someip_header(buf, original);

    SomeIpHeader parsed;
    ASSERT_TRUE(parse_someip_header(buf, 16, parsed));
    EXPECT_EQ(parsed.service_id, 0xFFFF);
    EXPECT_EQ(parsed.length, 0xFFFFFFFFu);
    EXPECT_EQ(parsed.message_type, SOMEIP_MSG_TYPE_RESPONSE);
}
