#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "body_control/lighting/domain/lamp_command_types.hpp"
#include "body_control/lighting/transport/someip_message_builder.hpp"
#include "body_control/lighting/transport/someip_message_parser.hpp"
#include "body_control/lighting/transport/transport_adapter_interface.hpp"

namespace
{

using body_control::lighting::domain::CommandSource;
using body_control::lighting::domain::IsValidLampCommand;
using body_control::lighting::domain::LampCommand;
using body_control::lighting::domain::LampCommandAction;
using body_control::lighting::domain::LampFunction;
using body_control::lighting::transport::SomeipMessageBuilder;
using body_control::lighting::transport::SomeipMessageParser;
using body_control::lighting::transport::TransportMessage;

// Wraps a raw payload in a TransportMessage shaped like a SetLampCommand
// request. ParseLampCommand only reads the payload field, so the header
// values exist solely to make the message structurally recognisable.
TransportMessage MakeLampCommandMsg(std::vector<std::uint8_t> payload)
{
    TransportMessage msg {};
    msg.service_id         = 0x5100U;
    msg.instance_id        = 0x0001U;
    msg.method_or_event_id = 0x0001U;
    msg.is_event           = false;
    msg.payload            = std::move(payload);
    return msg;
}

}  // namespace

// ── ParseLampCommand ──────────────────────────────────────────────────────────

TEST(SomeipMessageParserTest, ParseLampCommandHappyPath)
{
    // Wire layout: func(1) action(1) source(1) seq_hi(1) seq_lo(1)
    const TransportMessage msg =
        MakeLampCommandMsg({0x01U, 0x01U, 0x01U, 0x01U, 0x02U});
    const LampCommand cmd = SomeipMessageParser::ParseLampCommand(msg);

    EXPECT_EQ(cmd.function,         LampFunction::kLeftIndicator);
    EXPECT_EQ(cmd.action,           LampCommandAction::kActivate);
    EXPECT_EQ(cmd.source,           CommandSource::kHmiControlPanel);
    EXPECT_EQ(cmd.sequence_counter, 0x0102U);
}

TEST(SomeipMessageParserTest, ParseLampCommandShortPayloadReturnsSentinel)
{
    // ReadUint8 returns 0U on OOB — no throw, all fields zero-valued sentinel.
    const TransportMessage msg = MakeLampCommandMsg({0x00U, 0x00U});
    const LampCommand cmd = SomeipMessageParser::ParseLampCommand(msg);

    EXPECT_EQ(cmd.function,         LampFunction::kUnknown);
    EXPECT_EQ(cmd.action,           LampCommandAction::kNoAction);
    EXPECT_EQ(cmd.source,           CommandSource::kUnknown);
    EXPECT_EQ(cmd.sequence_counter, 0U);
}

TEST(SomeipMessageParserTest, ParseLampCommandRoundTripViaBuilder)
{
    LampCommand original {};
    original.function         = LampFunction::kHazardLamp;
    original.action           = LampCommandAction::kActivate;
    original.source           = CommandSource::kCentralZoneController;
    original.sequence_counter = 0xABCDU;

    const TransportMessage msg =
        SomeipMessageBuilder::BuildSetLampCommandRequest(original, 0x0001U, 0x0001U);
    const LampCommand parsed = SomeipMessageParser::ParseLampCommand(msg);

    EXPECT_EQ(parsed.function,         original.function);
    EXPECT_EQ(parsed.action,           original.action);
    EXPECT_EQ(parsed.source,           original.source);
    EXPECT_EQ(parsed.sequence_counter, original.sequence_counter);
}

// ── IsValidLampCommand ────────────────────────────────────────────────────────

TEST(SomeipMessageParserTest, IsValidLampCommandValidValuesReturnsTrue)
{
    LampCommand cmd {};
    cmd.function = LampFunction::kHeadLamp;               // max valid = 5
    cmd.action   = LampCommandAction::kToggle;            // max valid = 3
    cmd.source   = CommandSource::kCentralZoneController; // max valid = 3
    EXPECT_TRUE(IsValidLampCommand(cmd));
}

TEST(SomeipMessageParserTest, IsValidLampCommandOutOfRangeFunctionReturnsFalse)
{
    LampCommand cmd {};
    cmd.function = static_cast<LampFunction>(0xFFU);
    cmd.action   = LampCommandAction::kActivate;
    cmd.source   = CommandSource::kHmiControlPanel;
    EXPECT_FALSE(IsValidLampCommand(cmd));
}

TEST(SomeipMessageParserTest, IsValidLampCommandOutOfRangeActionReturnsFalse)
{
    LampCommand cmd {};
    cmd.function = LampFunction::kLeftIndicator;
    cmd.action   = static_cast<LampCommandAction>(0xFFU);
    cmd.source   = CommandSource::kHmiControlPanel;
    EXPECT_FALSE(IsValidLampCommand(cmd));
}

TEST(SomeipMessageParserTest, IsValidLampCommandOutOfRangeSourceReturnsFalse)
{
    LampCommand cmd {};
    cmd.function = LampFunction::kLeftIndicator;
    cmd.action   = LampCommandAction::kActivate;
    cmd.source   = static_cast<CommandSource>(0xFFU);
    EXPECT_FALSE(IsValidLampCommand(cmd));
}
