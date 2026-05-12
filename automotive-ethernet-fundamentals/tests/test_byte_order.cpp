#include <gtest/gtest.h>
#include "../common/byte_order.hpp"

// ============================================================
// read_u16_be — read a 16-bit big-endian value from a byte buffer
// ============================================================

// Basic: high byte at offset N, low byte at N+1.
TEST(ByteOrderTest, ReadU16BeBasic) {
    uint8_t buffer[] = {0x12, 0x34};
    EXPECT_EQ(read_u16_be(buffer, 0), 0x1234);
}

// Offset parameter is respected; sentinel bytes ignored.
TEST(ByteOrderTest, ReadU16BeAtOffset) {
    uint8_t buffer[] = {0xFF, 0x12, 0x34, 0xFF};
    EXPECT_EQ(read_u16_be(buffer, 1), 0x1234);
}

// Lower boundary: all-zero input → 0.
TEST(ByteOrderTest, ReadU16BeAllZeros) {
    uint8_t buffer[] = {0x00, 0x00};
    EXPECT_EQ(read_u16_be(buffer, 0), 0x0000);
}

// Upper boundary: all-ones input → 0xFFFF (no sign extension).
TEST(ByteOrderTest, ReadU16BeAllOnes) {
    uint8_t buffer[] = {0xFF, 0xFF};
    EXPECT_EQ(read_u16_be(buffer, 0), 0xFFFF);
}

// Endianness check: high-bit in LOW byte position must stay there.
// Catches accidental little-endian (would return 0x8000).
TEST(ByteOrderTest, ReadU16BeHighBitOnly) {
    uint8_t buffer[] = {0x00, 0x80};
    EXPECT_EQ(read_u16_be(buffer, 0), 0x0080);
}

// No sign extension when high byte has top bit set.
// A signed-char bug would produce 0xFFFFFF80 in a wider type.
TEST(ByteOrderTest, ReadU16BeHighByteTopBit) {
    uint8_t buffer[] = {0x80, 0x00};
    EXPECT_EQ(read_u16_be(buffer, 0), 0x8000);
}


// ============================================================
// read_u32_be — read a 32-bit big-endian value
// ============================================================

// Basic: four distinct bytes catch byte-swap bugs.
TEST(ByteOrderTest, ReadU32BeBasic) {
    uint8_t buffer[] = {0xDE, 0xAD, 0xBE, 0xEF};
    EXPECT_EQ(read_u32_be(buffer, 0), 0xDEADBEEFu);
}

// All-distinct ascending bytes verify each shift amount independently.
// Wrong shifts (e.g. <<16, <<8, <<4, <<0) would produce wrong result.
TEST(ByteOrderTest, ReadU32BeShiftAmounts) {
    uint8_t buffer[] = {0x01, 0x02, 0x03, 0x04};
    EXPECT_EQ(read_u32_be(buffer, 0), 0x01020304u);
}

// Upper boundary: all bits set, no sign extension.
TEST(ByteOrderTest, ReadU32BeAllOnes) {
    uint8_t buffer[] = {0xFF, 0xFF, 0xFF, 0xFF};
    EXPECT_EQ(read_u32_be(buffer, 0), 0xFFFFFFFFu);
}

// Offset parameter is respected; flanking sentinels ignored.
TEST(ByteOrderTest, ReadU32BeAtOffset) {
    uint8_t buffer[] = {0xAA, 0xAA, 0xDE, 0xAD, 0xBE, 0xEF, 0xAA, 0xAA};
    EXPECT_EQ(read_u32_be(buffer, 2), 0xDEADBEEFu);
}


// ============================================================
// write_u16_be — write a 16-bit value, big-endian
// ============================================================

// Basic: high byte at offset N, low byte at N+1.
TEST(ByteOrderTest, WriteU16BeBasic) {
    uint8_t buffer[2] = {};
    write_u16_be(buffer, 0, 0x1234);
    EXPECT_EQ(buffer[0], 0x12);
    EXPECT_EQ(buffer[1], 0x34);
}

// Lower boundary: zero write produces zero bytes.
// Pre-filled with 0xAA so a missing write would be detectable.
TEST(ByteOrderTest, WriteU16BeZero) {
    uint8_t buffer[2] = {0xAA, 0xAA};
    write_u16_be(buffer, 0, 0x0000);
    EXPECT_EQ(buffer[0], 0x00);
    EXPECT_EQ(buffer[1], 0x00);
}

// Upper boundary: max value.
TEST(ByteOrderTest, WriteU16BeAllOnes) {
    uint8_t buffer[2] = {};
    write_u16_be(buffer, 0, 0xFFFF);
    EXPECT_EQ(buffer[0], 0xFF);
    EXPECT_EQ(buffer[1], 0xFF);
}

// Neighbor safety: write must not disturb adjacent bytes.
// 0xAA sentinels around the target survive.
TEST(ByteOrderTest, WriteU16BeNeighborSafety) {
    uint8_t buffer[5] = {0xAA, 0xAA, 0xAA, 0xAA, 0xAA};
    write_u16_be(buffer, 1, 0x1234);
    EXPECT_EQ(buffer[0], 0xAA);
    EXPECT_EQ(buffer[1], 0x12);
    EXPECT_EQ(buffer[2], 0x34);
    EXPECT_EQ(buffer[3], 0xAA);
    EXPECT_EQ(buffer[4], 0xAA);
}


// ============================================================
// write_u32_be — write a 32-bit value, big-endian
// ============================================================

// Basic: four distinct bytes verify all shift positions.
TEST(ByteOrderTest, WriteU32BeBasic) {
    uint8_t buffer[4] = {};
    write_u32_be(buffer, 0, 0xCAFEBABEu);
    EXPECT_EQ(buffer[0], 0xCA);
    EXPECT_EQ(buffer[1], 0xFE);
    EXPECT_EQ(buffer[2], 0xBA);
    EXPECT_EQ(buffer[3], 0xBE);
}

// Lower boundary: zero write produces zero bytes.
TEST(ByteOrderTest, WriteU32BeZero) {
    uint8_t buffer[4] = {0xAA, 0xAA, 0xAA, 0xAA};
    write_u32_be(buffer, 0, 0x00000000u);
    EXPECT_EQ(buffer[0], 0x00);
    EXPECT_EQ(buffer[1], 0x00);
    EXPECT_EQ(buffer[2], 0x00);
    EXPECT_EQ(buffer[3], 0x00);
}

// Upper boundary: max value.
TEST(ByteOrderTest, WriteU32BeAllOnes) {
    uint8_t buffer[4] = {};
    write_u32_be(buffer, 0, 0xFFFFFFFFu);
    EXPECT_EQ(buffer[0], 0xFF);
    EXPECT_EQ(buffer[1], 0xFF);
    EXPECT_EQ(buffer[2], 0xFF);
    EXPECT_EQ(buffer[3], 0xFF);
}

// Neighbor safety: catches off-by-one writes of 3 or 5 bytes.
TEST(ByteOrderTest, WriteU32BeNeighborSafety) {
    uint8_t buffer[7] = {0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA};
    write_u32_be(buffer, 1, 0xDEADBEEFu);
    EXPECT_EQ(buffer[0], 0xAA);
    EXPECT_EQ(buffer[1], 0xDE);
    EXPECT_EQ(buffer[2], 0xAD);
    EXPECT_EQ(buffer[3], 0xBE);
    EXPECT_EQ(buffer[4], 0xEF);
    EXPECT_EQ(buffer[5], 0xAA);
    EXPECT_EQ(buffer[6], 0xAA);
}


// ============================================================
// Round-trip — write then read must recover the original value
// ============================================================

// 16-bit round-trip at offset 0.
TEST(ByteOrderTest, RoundTripU16) {
    uint8_t buffer[2] = {};
    write_u16_be(buffer, 0, 0xABCD);
    EXPECT_EQ(read_u16_be(buffer, 0), 0xABCD);
}

// 16-bit round-trip at non-zero offset.
TEST(ByteOrderTest, RoundTripU16AtOffset) {
    uint8_t buffer[10] = {};
    write_u16_be(buffer, 5, 0xABCD);
    EXPECT_EQ(read_u16_be(buffer, 5), 0xABCD);
}

// 16-bit round-trip with lower boundary value.
TEST(ByteOrderTest, RoundTripU16Zero) {
    uint8_t buffer[2] = {0xAA, 0xAA};
    write_u16_be(buffer, 0, 0x0000);
    EXPECT_EQ(read_u16_be(buffer, 0), 0x0000);
}

// 16-bit round-trip with upper boundary value.
TEST(ByteOrderTest, RoundTripU16Max) {
    uint8_t buffer[2] = {};
    write_u16_be(buffer, 0, 0xFFFF);
    EXPECT_EQ(read_u16_be(buffer, 0), 0xFFFF);
}

// 32-bit round-trip at offset 0.
TEST(ByteOrderTest, RoundTripU32) {
    uint8_t buffer[4] = {};
    write_u32_be(buffer, 0, 0x12345678u);
    EXPECT_EQ(read_u32_be(buffer, 0), 0x12345678u);
}

// 32-bit round-trip at non-zero offset.
TEST(ByteOrderTest, RoundTripU32AtOffset) {
    uint8_t buffer[10] = {};
    write_u32_be(buffer, 3, 0xCAFEBABEu);
    EXPECT_EQ(read_u32_be(buffer, 3), 0xCAFEBABEu);
}

// 32-bit round-trip with upper boundary value.
TEST(ByteOrderTest, RoundTripU32Max) {
    uint8_t buffer[4] = {};
    write_u32_be(buffer, 0, 0xFFFFFFFFu);
    EXPECT_EQ(read_u32_be(buffer, 0), 0xFFFFFFFFu);
}


// ============================================================
// Integration — combined usage mirroring real protocol code
// ============================================================

// Multiple back-to-back writes of mixed sizes must remain independent.
TEST(ByteOrderTest, MultipleAdjacentWrites) {
    uint8_t buffer[8] = {};
    write_u16_be(buffer, 0, 0x1234);
    write_u16_be(buffer, 2, 0x5678);
    write_u32_be(buffer, 4, 0xDEADBEEFu);

    EXPECT_EQ(read_u16_be(buffer, 0), 0x1234);
    EXPECT_EQ(read_u16_be(buffer, 2), 0x5678);
    EXPECT_EQ(read_u32_be(buffer, 4), 0xDEADBEEFu);

    EXPECT_EQ(buffer[0], 0x12);
    EXPECT_EQ(buffer[7], 0xEF);
}

// Full SOME/IP-style 16-byte header build and parse.
// Mirrors actual usage in someip_client.cpp / someip_server.cpp.
TEST(ByteOrderTest, SomeIpHeaderRoundTrip) {
    uint8_t buffer[16] = {};
    write_u16_be(buffer, 0, 0x5100);       // Service ID
    write_u16_be(buffer, 2, 0x0001);       // Method ID
    write_u32_be(buffer, 4, 0x0000000Cu);  // Length
    write_u16_be(buffer, 8, 0x0001);       // Client ID
    write_u16_be(buffer, 10, 0x0001);      // Session ID
    buffer[12] = 0x01;                     // Protocol Version
    buffer[13] = 0x01;                     // Interface Version
    buffer[14] = 0x00;                     // Message Type
    buffer[15] = 0x00;                     // Return Code

    EXPECT_EQ(read_u16_be(buffer, 0), 0x5100);
    EXPECT_EQ(read_u16_be(buffer, 2), 0x0001);
    EXPECT_EQ(read_u32_be(buffer, 4), 0x0000000Cu);
    EXPECT_EQ(read_u16_be(buffer, 8), 0x0001);
    EXPECT_EQ(read_u16_be(buffer, 10), 0x0001);
}

