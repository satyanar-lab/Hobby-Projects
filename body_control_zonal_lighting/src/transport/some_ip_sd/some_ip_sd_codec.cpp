// SOME/IP-SD wire format encoder/decoder.
//
// Uses the standard 16-byte SOME/IP header (Service ID 0xFFFF, Method ID 0x8100)
// as required by the AUTOSAR AP_PRS_SOMEIPServiceDiscovery specification and
// Wireshark's SOME/IP-SD dissector.  This is the only exception to the project's
// custom 20-byte header used for service-data traffic.
//
// References:
//   AUTOSAR AP_PRS_SOMEIPServiceDiscovery_SPECIFICATION
//   Wireshark packet-someipsd.c (expected option length 0x0009 for IPv4 endpoint)

#include "body_control/lighting/transport/some_ip_sd_codec.hpp"

#include <cstdio>
#include <cstring>

namespace body_control
{
namespace lighting
{
namespace transport
{
namespace SomeIpSdCodec
{

namespace
{

constexpr std::uint16_t kSdServiceId      {0xFFFFU};
constexpr std::uint16_t kSdMethodId       {0x8100U};
constexpr std::uint8_t  kProtoVersion     {0x01U};
constexpr std::uint8_t  kIfaceVersion     {0x01U};
constexpr std::uint8_t  kMsgTypeNotif     {0x02U};  // SOME/IP NOTIFICATION
constexpr std::uint8_t  kRetCodeOk        {0x00U};
constexpr std::uint8_t  kFlagsRebooting   {0xC0U};  // Reboot flag | Unicast flag
constexpr std::uint8_t  kEntryTypeFind    {0x00U};
constexpr std::uint8_t  kEntryTypeOffer   {0x01U};
constexpr std::uint8_t  kOptTypeIpv4Ep   {0x04U};  // IPv4 Endpoint Option type
constexpr std::uint8_t  kL4ProtoUdp       {0x11U};  // UDP = 17

constexpr std::uint32_t kEntrySize        {16U};
constexpr std::uint32_t kOptionSize       {12U};
// Per AUTOSAR spec and Wireshark source: option Length field = bytes AFTER the
// 2-byte Length field AND the 1-byte Type field.
constexpr std::uint16_t kOptContentLen    {0x0009U};  // 9 bytes: res+ip+res+proto+port

void AppendU16(std::vector<std::uint8_t>& v, const std::uint16_t x) noexcept
{
    v.push_back(static_cast<std::uint8_t>(x >> 8U));
    v.push_back(static_cast<std::uint8_t>(x & 0xFFU));
}

void AppendU32(std::vector<std::uint8_t>& v, const std::uint32_t x) noexcept
{
    v.push_back(static_cast<std::uint8_t>(x >> 24U));
    v.push_back(static_cast<std::uint8_t>(x >> 16U));
    v.push_back(static_cast<std::uint8_t>(x >>  8U));
    v.push_back(static_cast<std::uint8_t>(x & 0xFFU));
}

void AppendSomeipHeader(
    std::vector<std::uint8_t>& v,
    const std::uint32_t        sd_payload_len,
    const std::uint16_t        session_id) noexcept
{
    // SOME/IP Length field = total_frame_size - 8
    // = (16-byte header + sd_payload) - 8
    // = 8 (bytes 8-15) + sd_payload_len
    const std::uint32_t length = 8U + sd_payload_len;
    AppendU16(v, kSdServiceId);
    AppendU16(v, kSdMethodId);
    AppendU32(v, length);
    AppendU16(v, 0x0000U);  // client ID (not used in SD)
    AppendU16(v, session_id);
    v.push_back(kProtoVersion);
    v.push_back(kIfaceVersion);
    v.push_back(kMsgTypeNotif);
    v.push_back(kRetCodeOk);
}

// Minimal dotted-decimal parser — avoids inet_pton dependency in pure-codec code.
bool ParseIpv4(const char* const ip_str, std::uint8_t out[4]) noexcept
{
    unsigned int a = 0U, b = 0U, c = 0U, d = 0U;
    // NOLINTNEXTLINE(cert-err34-c): no exception path; validated range check below
    if (std::sscanf(ip_str, "%u.%u.%u.%u", &a, &b, &c, &d) != 4) { return false; }
    if ((a > 255U) || (b > 255U) || (c > 255U) || (d > 255U))    { return false; }
    out[0] = static_cast<std::uint8_t>(a);
    out[1] = static_cast<std::uint8_t>(b);
    out[2] = static_cast<std::uint8_t>(c);
    out[3] = static_cast<std::uint8_t>(d);
    return true;
}

std::uint16_t ReadU16Be(const std::uint8_t* const p) noexcept
{
    return static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(p[0]) << 8U) | p[1]);
}

}  // namespace

std::vector<std::uint8_t> EncodeOffer(
    const ServiceOffer& offer,
    const std::uint16_t session_id) noexcept
{
    // SD payload: flags+reserved(4) + entries_len(4) + entry(16) + options_len(4) + option(12) = 40
    // SOME/IP length field: 8 + 40 = 48 = 0x30
    // Total frame: 16 + 40 = 56 bytes
    constexpr std::uint32_t kSdPayloadLen {4U + 4U + kEntrySize + 4U + kOptionSize};

    std::vector<std::uint8_t> frame;
    frame.reserve(16U + kSdPayloadLen);

    AppendSomeipHeader(frame, kSdPayloadLen, session_id);

    // SD flags + 3 reserved bytes
    frame.push_back(kFlagsRebooting);
    frame.push_back(0x00U);
    frame.push_back(0x00U);
    frame.push_back(0x00U);

    // Entries array
    AppendU32(frame, kEntrySize);     // entries_length = 16
    frame.push_back(kEntryTypeOffer); // entry type = 0x01
    frame.push_back(0x00U);           // index first = 0 (option run 1 starts at option[0])
    frame.push_back(0x00U);           // index second = 0 (unused)
    frame.push_back(0x10U);           // num_opts: (1<<4)|0 = 1 in first run, 0 in second
    AppendU16(frame, offer.service_id);
    AppendU16(frame, offer.instance_id);
    frame.push_back(offer.major_version);
    // TTL: 3 bytes big-endian
    frame.push_back(static_cast<std::uint8_t>((offer.ttl_seconds >> 16U) & 0xFFU));
    frame.push_back(static_cast<std::uint8_t>((offer.ttl_seconds >>  8U) & 0xFFU));
    frame.push_back(static_cast<std::uint8_t>( offer.ttl_seconds         & 0xFFU));
    AppendU32(frame, offer.minor_version);

    // Options array: one IPv4 Endpoint Option (12 bytes)
    AppendU32(frame, kOptionSize);  // options_length = 12

    // IPv4 Endpoint Option layout (12 bytes total):
    // [0-1] Length (= kOptContentLen = 9, bytes after Type field)
    // [2]   Type   (0x04)
    // [3]   Reserved
    // [4-7] IPv4 address
    // [8]   Reserved
    // [9]   L4 protocol (0x11 = UDP)
    // [10-11] Port
    AppendU16(frame, kOptContentLen);  // 0x0009
    frame.push_back(kOptTypeIpv4Ep);   // 0x04
    frame.push_back(0x00U);            // reserved

    std::uint8_t ip_bytes[4] {};
    if (!ParseIpv4(offer.local_ip, ip_bytes))
    {
        ip_bytes[0] = ip_bytes[1] = ip_bytes[2] = ip_bytes[3] = 0U;
    }
    frame.push_back(ip_bytes[0]);
    frame.push_back(ip_bytes[1]);
    frame.push_back(ip_bytes[2]);
    frame.push_back(ip_bytes[3]);
    frame.push_back(0x00U);         // reserved
    frame.push_back(kL4ProtoUdp);   // 0x11
    AppendU16(frame, offer.local_port);

    return frame;
}

std::vector<std::uint8_t> EncodeFind(
    const std::uint16_t service_id,
    const std::uint16_t instance_id,
    const std::uint16_t session_id) noexcept
{
    // SD payload: flags+reserved(4) + entries_len(4) + entry(16) + options_len(4) = 28
    // SOME/IP length field: 8 + 28 = 36 = 0x24
    // Total frame: 16 + 28 = 44 bytes
    constexpr std::uint32_t kSdPayloadLen {4U + 4U + kEntrySize + 4U};

    std::vector<std::uint8_t> frame;
    frame.reserve(16U + kSdPayloadLen);

    AppendSomeipHeader(frame, kSdPayloadLen, session_id);

    frame.push_back(kFlagsRebooting);
    frame.push_back(0x00U);
    frame.push_back(0x00U);
    frame.push_back(0x00U);

    AppendU32(frame, kEntrySize);    // entries_length = 16
    frame.push_back(kEntryTypeFind); // type = 0x00
    frame.push_back(0x00U);          // index first
    frame.push_back(0x00U);          // index second
    frame.push_back(0x00U);          // 0 options
    AppendU16(frame, service_id);
    AppendU16(frame, instance_id);
    frame.push_back(0xFFU);           // major version = 0xFF (any)
    // TTL = 1 for FindService
    frame.push_back(0x00U);
    frame.push_back(0x00U);
    frame.push_back(0x01U);
    AppendU32(frame, 0xFFFFFFFFU);    // minor version = any

    AppendU32(frame, 0x00000000U);    // options_length = 0

    return frame;
}

bool DecodeOffer(
    const std::uint8_t* const data,
    const std::size_t         len,
    const std::uint16_t       wanted_service_id,
    const std::uint16_t       wanted_instance_id,
    DiscoveredService&         out) noexcept
{
    // Minimum: 16-byte header + 4 (flags/reserved) + 4 (entries_len) + 16 (entry) +
    //          4 (options_len) + 12 (option) = 56 bytes
    constexpr std::size_t kMinLen {56U};
    constexpr std::size_t kHeaderLen {16U};

    if ((data == nullptr) || (len < kMinLen)) { return false; }

    // Validate SOME/IP-SD magic bytes
    if ((ReadU16Be(data + 0U) != kSdServiceId) ||
        (ReadU16Be(data + 2U) != kSdMethodId)) { return false; }

    // SD payload starts at byte 16
    const std::uint8_t* const sd     = data + kHeaderLen;
    const std::size_t          sd_len = len  - kHeaderLen;

    if (sd_len < 8U) { return false; }  // need flags(4) + entries_len(4)

    const std::uint32_t entries_len =
        (static_cast<std::uint32_t>(sd[4]) << 24U) |
        (static_cast<std::uint32_t>(sd[5]) << 16U) |
        (static_cast<std::uint32_t>(sd[6]) <<  8U) |
         static_cast<std::uint32_t>(sd[7]);

    if ((entries_len == 0U) || ((8U + entries_len) > sd_len)) { return false; }

    const std::uint32_t num_entries = entries_len / kEntrySize;

    for (std::uint32_t i = 0U; i < num_entries; ++i)
    {
        const std::uint8_t* const entry = sd + 8U + (static_cast<std::size_t>(i) * kEntrySize);
        if (entry[0] != kEntryTypeOffer) { continue; }  // not OfferService

        const std::uint16_t svc  = ReadU16Be(entry + 4U);
        const std::uint16_t inst = ReadU16Be(entry + 6U);
        if ((svc != wanted_service_id) || (inst != wanted_instance_id)) { continue; }

        out.service_id    = svc;
        out.instance_id   = inst;
        out.major_version = entry[8U];
        // remote_ip and remote_port default to empty unless we find an IPv4 option.
        out.remote_ip[0]  = '\0';
        out.remote_port   = 0U;

        // Try to read the IPv4 Endpoint Option that follows the entries array.
        const std::size_t opts_start = 8U + entries_len;
        if (opts_start + 4U > sd_len) { return true; }  // no options section

        const std::uint32_t opts_len =
            (static_cast<std::uint32_t>(sd[opts_start + 0U]) << 24U) |
            (static_cast<std::uint32_t>(sd[opts_start + 1U]) << 16U) |
            (static_cast<std::uint32_t>(sd[opts_start + 2U]) <<  8U) |
             static_cast<std::uint32_t>(sd[opts_start + 3U]);

        if (opts_len < kOptionSize) { return true; }  // options present but too short

        const std::uint8_t* const opt = sd + opts_start + 4U;
        // opt[2] = type
        if (opt[2] != kOptTypeIpv4Ep) { return true; }  // not IPv4 endpoint option

        // IPv4 address at opt[4..7], protocol at opt[9], port at opt[10..11]
        static_cast<void>(std::snprintf(
            out.remote_ip, sizeof(out.remote_ip),
            "%u.%u.%u.%u",
            static_cast<unsigned>(opt[4]),
            static_cast<unsigned>(opt[5]),
            static_cast<unsigned>(opt[6]),
            static_cast<unsigned>(opt[7])));
        out.remote_port = ReadU16Be(opt + 10U);

        return true;
    }

    return false;
}

}  // namespace SomeIpSdCodec
}  // namespace transport
}  // namespace lighting
}  // namespace body_control
