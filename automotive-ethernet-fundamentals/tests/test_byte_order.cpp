#include <gtest/gtest.h>
#include "../common/byte_order.hpp"

// ----- read_u16_be tests -----

// Test that read_u16_be combines two bytes correctly
TEST(ByteOrderTest, ReadU16BeBasic) {
    uint8_t buffer[] = {0x12, 0x34};
    uint16_t result = read_u16_be(buffer, 0);
    EXPECT_EQ(result, 0x1234);
}

// Verifies the function reads at the specified offset, not always at 0.
// The 0xFF sentinel bytes at offsets 0 and 3 should be ignored;
// reading at offset 1 must return the value built from {0x12, 0x34}.
TEST(ByteOrderTest, ReadU16BeAtOffset) {
    uint8_t buffer[] = {0xFF, 0x12, 0x34, 0xFF};
    EXPECT_EQ(read_u16_be(buffer, 1), 0x1234);
}

// Verifies the upper boundary: 0xFFFF must be returned exactly,
// not corrupted by sign extension or masking errors.
TEST(ByteOrderTest, ReadU16BeAllOnes) {
    uint8_t buffer[] = {0xFF, 0xFF};
    EXPECT_EQ(read_u16_be(buffer, 0), 0xFFFF);
}

// Verifies the lower boundary: all-zero input must yield 0,
// not garbage from uninitialized memory or wrong shifts.
TEST(ByteOrderTest, ReadU16BeAllZeros) {
    uint8_t buffer[] = {0x00, 0x00};
    EXPECT_EQ(read_u16_be(buffer, 0), 0x0000);
}

// ----- read_u32_be tests -----

// Verifies 32-bit big-endian read with four distinct, asymmetric bytes.
// Using {0xDE, 0xAD, 0xBE, 0xEF} (all different) ensures any byte
// swap or wrong shift amount in the implementation would be caught.
TEST(ByteOrderTest, ReadU32BeBasic) {
    uint8_t buffer[] = {0xDE, 0xAD, 0xBE, 0xEF};
    EXPECT_EQ(read_u32_be(buffer, 0), 0xDEADBEEF);
}

// ----- write_u16_be tests -----

// Verifies that writing 0x1234 places the high byte at offset 0
// and the low byte at offset 1 — confirming big-endian order.
// Buffer is value-initialized to zero so any spurious writes
// would show up as non-zero in adjacent slots.
TEST(ByteOrderTest, WriteU16BeBasic) {
    uint8_t buffer[2] = {};
    write_u16_be(buffer, 0, 0x1234);
    EXPECT_EQ(buffer[0], 0x12);
    EXPECT_EQ(buffer[1], 0x34);
}

// ----- write_u32_be tests -----

// Verifies 32-bit big-endian write places each byte in the correct
// position. 0xCAFEBABE has four distinct bytes so any swap is caught.
TEST(ByteOrderTest, WriteU32BeBasic) {
    uint8_t buffer[4] = {};
    write_u32_be(buffer, 0, 0xCAFEBABE);
    EXPECT_EQ(buffer[0], 0xCA);
    EXPECT_EQ(buffer[1], 0xFE);
    EXPECT_EQ(buffer[2], 0xBA);
    EXPECT_EQ(buffer[3], 0xBE);
}

// ----- Round-trip tests (most valuable kind) -----

// Verifies that write_u16_be and read_u16_be are inverses of each other.
// Even if both functions were wrong in the same way, this test would
// catch some bugs — e.g., if both used little-endian internally,
// they'd round-trip but the bytes in the buffer would still be wrong.
// (Caught by WriteU16BeBasic above.) Combined, the tests verify
// independent correctness AND mutual consistency.
TEST(ByteOrderTest, RoundTripU16) {
    uint8_t buffer[2] = {};
    write_u16_be(buffer, 0, 0xABCD);
    EXPECT_EQ(read_u16_be(buffer, 0), 0xABCD);
}

// Same round-trip principle for 32-bit values.
// 0x12345678 has all-distinct bytes so byte-swap bugs are caught.
TEST(ByteOrderTest, RoundTripU32) {
    uint8_t buffer[4] = {};
    write_u32_be(buffer, 0, 0x12345678);
    EXPECT_EQ(read_u32_be(buffer, 0), 0x12345678);
}
