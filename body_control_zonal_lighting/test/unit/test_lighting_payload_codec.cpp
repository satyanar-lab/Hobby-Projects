/**
 * @file test_lighting_payload_codec.cpp
 * @brief Unit tests for the domain payload codec.
 *
 * Covered:
 *   1. Round-trip for LampCommand, LampStatus, NodeHealthStatus.
 *   2. Rejection of invalid encoded enum values on decode.
 *   3. Rejection of wrong-length payloads.
 *   4. Rejection of null payload pointer.
 */

#include <array>
#include <cstdint>

#include <gtest/gtest.h>

#include "body_control/lighting/domain/lighting_payload_codec.hpp"

namespace
{

using body_control::lighting::domain::CommandSource;
using body_control::lighting::domain::DecodeLampCommand;
using body_control::lighting::domain::DecodeLampStatus;
using body_control::lighting::domain::DecodeNodeHealthStatus;
using body_control::lighting::domain::EncodeLampCommand;
using body_control::lighting::domain::EncodeLampStatus;
using body_control::lighting::domain::EncodeNodeHealthStatus;
using body_control::lighting::domain::kLampCommandPayloadLength;
using body_control::lighting::domain::kLampStatusPayloadLength;
using body_control::lighting::domain::kNodeHealthStatusPayloadLength;
using body_control::lighting::domain::LampCommand;
using body_control::lighting::domain::LampCommandAction;
using body_control::lighting::domain::LampCommandPayloadBuffer;
using body_control::lighting::domain::LampFunction;
using body_control::lighting::domain::LampOutputState;
using body_control::lighting::domain::LampStatus;
using body_control::lighting::domain::LampStatusPayloadBuffer;
using body_control::lighting::domain::NodeHealthState;
using body_control::lighting::domain::NodeHealthStatus;
using body_control::lighting::domain::NodeHealthStatusPayloadBuffer;
using body_control::lighting::domain::PayloadCodecStatus;

}  // namespace

TEST(LightingPayloadCodecTest, LampCommandRoundTrip)
{
    LampCommand original {};
    original.function = LampFunction::kHazardLamp;
    original.action = LampCommandAction::kActivate;
    original.source = CommandSource::kHmiControlPanel;
    original.sequence_counter = 0x1234U;

    LampCommandPayloadBuffer buffer {};
    ASSERT_EQ(EncodeLampCommand(original, buffer),
              PayloadCodecStatus::kSuccess);

    LampCommand decoded {};
    ASSERT_EQ(
        DecodeLampCommand(buffer.data(), buffer.size(), decoded),
        PayloadCodecStatus::kSuccess);

    EXPECT_EQ(decoded.function, original.function);
    EXPECT_EQ(decoded.action, original.action);
    EXPECT_EQ(decoded.source, original.source);
    EXPECT_EQ(decoded.sequence_counter, original.sequence_counter);
}

TEST(LightingPayloadCodecTest, LampCommandDecodeRejectsWrongLength)
{
    const std::array<std::uint8_t, 4U> too_short {};
    LampCommand decoded {};
    EXPECT_EQ(
        DecodeLampCommand(too_short.data(), too_short.size(), decoded),
        PayloadCodecStatus::kInvalidPayloadLength);
}

TEST(LightingPayloadCodecTest, LampCommandDecodeRejectsNullPointer)
{
    LampCommand decoded {};
    EXPECT_EQ(
        DecodeLampCommand(nullptr, kLampCommandPayloadLength, decoded),
        PayloadCodecStatus::kInvalidPayloadLength);
}

TEST(LightingPayloadCodecTest, LampCommandDecodeRejectsInvalidEnum)
{
    // Byte 0 is LampFunction; 0xFF is outside the defined range.
    std::array<std::uint8_t, kLampCommandPayloadLength> bad {};
    bad[0] = 0xFFU;

    LampCommand decoded {};
    EXPECT_EQ(
        DecodeLampCommand(bad.data(), bad.size(), decoded),
        PayloadCodecStatus::kInvalidPayloadValue);
}

TEST(LightingPayloadCodecTest, LampStatusRoundTrip)
{
    LampStatus original {};
    original.function = LampFunction::kLeftIndicator;
    original.output_state = LampOutputState::kOn;
    original.command_applied = true;
    original.last_sequence_counter = 0xBEEFU;

    LampStatusPayloadBuffer buffer {};
    ASSERT_EQ(EncodeLampStatus(original, buffer),
              PayloadCodecStatus::kSuccess);

    LampStatus decoded {};
    ASSERT_EQ(
        DecodeLampStatus(buffer.data(), buffer.size(), decoded),
        PayloadCodecStatus::kSuccess);

    EXPECT_EQ(decoded.function, original.function);
    EXPECT_EQ(decoded.output_state, original.output_state);
    EXPECT_TRUE(decoded.command_applied);
    EXPECT_EQ(decoded.last_sequence_counter, original.last_sequence_counter);
}

TEST(LightingPayloadCodecTest, LampStatusDecodeRejectsInvalidBooleanByte)
{
    // command_applied is at byte 2 and must be 0 or 1.
    std::array<std::uint8_t, kLampStatusPayloadLength> bad {};
    bad[0] = static_cast<std::uint8_t>(LampFunction::kHeadLamp);
    bad[1] = static_cast<std::uint8_t>(LampOutputState::kOff);
    bad[2] = 0xAAU;

    LampStatus decoded {};
    EXPECT_EQ(
        DecodeLampStatus(bad.data(), bad.size(), decoded),
        PayloadCodecStatus::kInvalidPayloadValue);
}

TEST(LightingPayloadCodecTest, NodeHealthStatusRoundTrip)
{
    NodeHealthStatus original {};
    original.health_state = NodeHealthState::kDegraded;
    original.ethernet_link_available = true;
    original.service_available = false;
    original.lamp_driver_fault_present = true;
    original.active_fault_count = 7U;

    NodeHealthStatusPayloadBuffer buffer {};
    ASSERT_EQ(EncodeNodeHealthStatus(original, buffer),
              PayloadCodecStatus::kSuccess);

    NodeHealthStatus decoded {};
    ASSERT_EQ(
        DecodeNodeHealthStatus(buffer.data(), buffer.size(), decoded),
        PayloadCodecStatus::kSuccess);

    EXPECT_EQ(decoded.health_state, original.health_state);
    EXPECT_TRUE(decoded.ethernet_link_available);
    EXPECT_FALSE(decoded.service_available);
    EXPECT_TRUE(decoded.lamp_driver_fault_present);
    EXPECT_EQ(decoded.active_fault_count, original.active_fault_count);
}

TEST(LightingPayloadCodecTest, NodeHealthStatusDecodeRejectsInvalidEnum)
{
    std::array<std::uint8_t, kNodeHealthStatusPayloadLength> bad {};
    bad[0] = 0xEEU;  // out-of-range health_state

    NodeHealthStatus decoded {};
    EXPECT_EQ(
        DecodeNodeHealthStatus(bad.data(), bad.size(), decoded),
        PayloadCodecStatus::kInvalidPayloadValue);
}

TEST(LightingPayloadCodecTest, NodeHealthStatusDecodeRejectsWrongLength)
{
    const std::array<std::uint8_t, 3U> too_short {};
    NodeHealthStatus decoded {};
    EXPECT_EQ(
        DecodeNodeHealthStatus(too_short.data(), too_short.size(), decoded),
        PayloadCodecStatus::kInvalidPayloadLength);
}
