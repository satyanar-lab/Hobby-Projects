#include <gtest/gtest.h>

#include <cstring>

#include "body_control/lighting/transport/some_ip_sd_codec.hpp"
#include "body_control/lighting/transport/some_ip_sd_types.hpp"

using body_control::lighting::transport::DiscoveredService;
using body_control::lighting::transport::ServiceOffer;
using body_control::lighting::transport::SomeIpSdCodec::DecodeOffer;
using body_control::lighting::transport::SomeIpSdCodec::EncodeFind;
using body_control::lighting::transport::SomeIpSdCodec::EncodeOffer;

static ServiceOffer MakeOffer(
    const std::uint16_t svc_id  = 0x5100U,
    const std::uint16_t inst_id = 0x0001U,
    const char*         ip      = "192.168.0.20",
    const std::uint16_t port    = 41001U) noexcept
{
    ServiceOffer o {};
    o.service_id    = svc_id;
    o.instance_id   = inst_id;
    o.major_version = 1U;
    o.minor_version = 0U;
    o.ttl_seconds   = 5U;
    o.local_port    = port;
    std::strncpy(o.local_ip, ip, sizeof(o.local_ip) - 1U);
    return o;
}

// ── EncodeOffer ───────────────────────────────────────────────────────────────

TEST(SomeIpSdCodecTest, EncodeOfferProduces56Bytes)
{
    const auto frame = EncodeOffer(MakeOffer(), 0x0001U);
    EXPECT_EQ(frame.size(), 56U);
}

TEST(SomeIpSdCodecTest, EncodeOfferHasSomeIpSdMagicBytes)
{
    const auto frame = EncodeOffer(MakeOffer(), 0x0001U);
    // Service ID = 0xFFFF
    EXPECT_EQ(frame[0], 0xFFU);
    EXPECT_EQ(frame[1], 0xFFU);
    // Method ID = 0x8100
    EXPECT_EQ(frame[2], 0x81U);
    EXPECT_EQ(frame[3], 0x00U);
}

TEST(SomeIpSdCodecTest, EncodeOfferLengthFieldIs48)
{
    // SOME/IP Length = total_frame (56) - 8 = 48 = 0x00000030
    const auto frame = EncodeOffer(MakeOffer(), 0x0001U);
    const std::uint32_t length =
        (static_cast<std::uint32_t>(frame[4]) << 24U) |
        (static_cast<std::uint32_t>(frame[5]) << 16U) |
        (static_cast<std::uint32_t>(frame[6]) <<  8U) |
         static_cast<std::uint32_t>(frame[7]);
    EXPECT_EQ(length, 48U);
}

TEST(SomeIpSdCodecTest, EncodeOfferEntryTypeIsOffer)
{
    const auto frame = EncodeOffer(MakeOffer(), 0x0001U);
    // SD payload starts at byte 16: flags(1)+reserved(3)+entries_len(4) = 8 bytes before entry
    // Entry type is at frame[16 + 4 + 4] = frame[24]
    EXPECT_EQ(frame[24], 0x01U);  // OfferService entry type
}

TEST(SomeIpSdCodecTest, EncodeOfferServiceIdInEntry)
{
    const auto frame = EncodeOffer(MakeOffer(0x5100U, 0x0001U), 0x0001U);
    // Entry starts at frame[24]; service_id at entry[4..5] = frame[28..29]
    const std::uint16_t svc =
        (static_cast<std::uint16_t>(frame[28]) << 8U) | frame[29];
    EXPECT_EQ(svc, 0x5100U);
}

TEST(SomeIpSdCodecTest, EncodeOfferIpv4OptionType)
{
    const auto frame = EncodeOffer(MakeOffer(), 0x0001U);
    // Option starts at frame[16+4+4+16+4] = frame[44]
    // opt[2] = type = 0x04
    EXPECT_EQ(frame[44 + 2], 0x04U);
}

TEST(SomeIpSdCodecTest, EncodeOfferPortInOption)
{
    const auto frame = EncodeOffer(MakeOffer(0x5100U, 0x0001U, "192.168.0.20", 41001U), 0x0001U);
    // opt[10..11] = port 41001 = 0xA029
    const std::uint16_t port =
        (static_cast<std::uint16_t>(frame[44 + 10]) << 8U) | frame[44 + 11];
    EXPECT_EQ(port, 41001U);
}

// ── EncodeFind ────────────────────────────────────────────────────────────────

TEST(SomeIpSdCodecTest, EncodeFindProduces44Bytes)
{
    const auto frame = EncodeFind(0x5100U, 0x0001U, 0x0001U);
    EXPECT_EQ(frame.size(), 44U);
}

TEST(SomeIpSdCodecTest, EncodeFindEntryTypeIsFind)
{
    const auto frame = EncodeFind(0x5100U, 0x0001U, 0x0001U);
    // Entry at frame[24], type at [24]
    EXPECT_EQ(frame[24], 0x00U);  // FindService
}

TEST(SomeIpSdCodecTest, EncodeFindOptionsLengthIsZero)
{
    const auto frame = EncodeFind(0x5100U, 0x0001U, 0x0001U);
    // Options length at frame[16+4+4+16] = frame[40..43]
    const std::uint32_t opts_len =
        (static_cast<std::uint32_t>(frame[40]) << 24U) |
        (static_cast<std::uint32_t>(frame[41]) << 16U) |
        (static_cast<std::uint32_t>(frame[42]) <<  8U) |
         static_cast<std::uint32_t>(frame[43]);
    EXPECT_EQ(opts_len, 0U);
}

// ── DecodeOffer ───────────────────────────────────────────────────────────────

TEST(SomeIpSdCodecTest, DecodeOfferRoundTrip)
{
    const ServiceOffer offer = MakeOffer(0x5100U, 0x0001U, "192.168.0.20", 41001U);
    const auto frame = EncodeOffer(offer, 0x0001U);

    DiscoveredService svc {};
    ASSERT_TRUE(DecodeOffer(frame.data(), frame.size(), 0x5100U, 0x0001U, svc));

    EXPECT_EQ(svc.service_id,    0x5100U);
    EXPECT_EQ(svc.instance_id,   0x0001U);
    EXPECT_EQ(svc.major_version, 1U);
    EXPECT_STREQ(svc.remote_ip,  "192.168.0.20");
    EXPECT_EQ(svc.remote_port,   41001U);
}

TEST(SomeIpSdCodecTest, DecodeOfferWrongServiceIdReturnsFalse)
{
    const auto frame = EncodeOffer(MakeOffer(0x5100U, 0x0001U), 0x0001U);
    DiscoveredService svc {};
    // Looking for 0x5200 but frame has 0x5100
    EXPECT_FALSE(DecodeOffer(frame.data(), frame.size(), 0x5200U, 0x0001U, svc));
}

TEST(SomeIpSdCodecTest, DecodeOfferTruncatedFrameReturnsFalse)
{
    const auto frame = EncodeOffer(MakeOffer(), 0x0001U);
    DiscoveredService svc {};
    EXPECT_FALSE(DecodeOffer(frame.data(), 20U, 0x5100U, 0x0001U, svc));
}

TEST(SomeIpSdCodecTest, DecodeOfferNullDataReturnsFalse)
{
    DiscoveredService svc {};
    EXPECT_FALSE(DecodeOffer(nullptr, 56U, 0x5100U, 0x0001U, svc));
}

TEST(SomeIpSdCodecTest, DecodeOfferWrongServiceIdHeaderReturnsFalse)
{
    // Corrupt the SOME/IP Service ID field (bytes 0-1) — should fail magic check
    auto frame = EncodeOffer(MakeOffer(), 0x0001U);
    frame[0] = 0x51U;  // not 0xFF
    DiscoveredService svc {};
    EXPECT_FALSE(DecodeOffer(frame.data(), frame.size(), 0x5100U, 0x0001U, svc));
}
