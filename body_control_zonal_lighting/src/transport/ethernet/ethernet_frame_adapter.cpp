#include <arpa/inet.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "body_control/lighting/transport/transport_adapter_interface.hpp"

namespace body_control
{
namespace lighting
{
namespace transport
{
namespace ethernet
{

namespace
{

// Version field value written on encode and validated on decode.  A mismatched
// version causes decode to return false so stale frames from an old build are
// rejected rather than misinterpreted.
constexpr std::uint16_t kFrameVersion {1U};

// Bit positions within the flags field.  Two bits are currently defined;
// remaining bits are reserved and must be written as 0.
constexpr std::uint16_t kFlagIsEventMask {0x0001U};
constexpr std::uint16_t kFlagIsReliableMask {0x0002U};

// Wire layout (all fields big-endian, total size = 20 bytes):
//   [0-1]   version
//   [2-3]   message_kind  (1=request, 2=response, 3=event)
//   [4-5]   service_id
//   [6-7]   instance_id
//   [8-9]   method_or_event_id
//   [10-11] client_id
//   [12-13] session_id
//   [14-15] flags         (bit0=is_event, bit1=is_reliable)
//   [16-19] payload_length (bytes, not including header)
struct EthernetFrameHeader
{
    std::uint16_t version;
    std::uint16_t message_kind;
    std::uint16_t service_id;
    std::uint16_t instance_id;
    std::uint16_t method_or_event_id;
    std::uint16_t client_id;
    std::uint16_t session_id;
    std::uint16_t flags;
    std::uint32_t payload_length;
};

constexpr std::size_t kEthernetFrameHeaderSize = sizeof(EthernetFrameHeader);

// Packs the boolean TransportMessage fields into a single flags word so the
// wire format stays fixed-width regardless of how many boolean fields exist.
std::uint16_t BuildFlags(
    const TransportMessage& transport_message) noexcept
{
    std::uint16_t flags {0U};

    if (transport_message.is_event)
    {
        flags =
            static_cast<std::uint16_t>(flags | kFlagIsEventMask);
    }

    if (transport_message.is_reliable)
    {
        flags =
            static_cast<std::uint16_t>(flags | kFlagIsReliableMask);
    }

    return flags;
}

// Converts every multi-byte header field to network byte order in-place.
// Called immediately before memcpy-ing the header into the frame buffer.
void EncodeHeaderToNetworkOrder(
    EthernetFrameHeader& frame_header) noexcept
{
    frame_header.version = htons(frame_header.version);
    frame_header.message_kind = htons(frame_header.message_kind);
    frame_header.service_id = htons(frame_header.service_id);
    frame_header.instance_id = htons(frame_header.instance_id);
    frame_header.method_or_event_id = htons(frame_header.method_or_event_id);
    frame_header.client_id = htons(frame_header.client_id);
    frame_header.session_id = htons(frame_header.session_id);
    frame_header.flags = htons(frame_header.flags);
    frame_header.payload_length = htonl(frame_header.payload_length);
}

// Converts every multi-byte header field from network byte order to host order.
// Called immediately after memcpy-ing the raw received bytes into the struct.
void DecodeHeaderFromNetworkOrder(
    EthernetFrameHeader& frame_header) noexcept
{
    frame_header.version = ntohs(frame_header.version);
    frame_header.message_kind = ntohs(frame_header.message_kind);
    frame_header.service_id = ntohs(frame_header.service_id);
    frame_header.instance_id = ntohs(frame_header.instance_id);
    frame_header.method_or_event_id = ntohs(frame_header.method_or_event_id);
    frame_header.client_id = ntohs(frame_header.client_id);
    frame_header.session_id = ntohs(frame_header.session_id);
    frame_header.flags = ntohs(frame_header.flags);
    frame_header.payload_length = ntohl(frame_header.payload_length);
}

}  // namespace

std::vector<std::uint8_t> EncodeTransportMessage(
    const TransportMessage& transport_message,
    const std::uint16_t message_kind)
{
    const std::size_t payload_length = transport_message.payload.size();
    const std::size_t frame_length = kEthernetFrameHeaderSize + payload_length;

    std::vector<std::uint8_t> frame_bytes(frame_length, 0U);

    EthernetFrameHeader frame_header {};
    frame_header.version = kFrameVersion;
    frame_header.message_kind = message_kind;
    frame_header.service_id = transport_message.service_id;
    frame_header.instance_id = transport_message.instance_id;
    frame_header.method_or_event_id = transport_message.method_or_event_id;
    frame_header.client_id = transport_message.client_id;
    frame_header.session_id = transport_message.session_id;
    frame_header.flags = BuildFlags(transport_message);
    frame_header.payload_length =
        static_cast<std::uint32_t>(payload_length);

    EncodeHeaderToNetworkOrder(frame_header);

    std::memcpy(
        frame_bytes.data(),
        &frame_header,
        kEthernetFrameHeaderSize);

    if (payload_length > 0U)
    {
        std::memcpy(
            frame_bytes.data() + kEthernetFrameHeaderSize,
            transport_message.payload.data(),
            payload_length);
    }

    return frame_bytes;
}

bool DecodeTransportMessage(
    const std::uint8_t* const frame_data,
    const std::size_t frame_length,
    TransportMessage& transport_message,
    std::uint16_t& message_kind) noexcept
{
    if ((frame_data == nullptr) || (frame_length < kEthernetFrameHeaderSize))
    {
        return false;
    }

    EthernetFrameHeader frame_header {};
    std::memcpy(
        &frame_header,
        frame_data,
        kEthernetFrameHeaderSize);

    DecodeHeaderFromNetworkOrder(frame_header);

    // Reject frames from a different wire-format version before reading any
    // other fields whose layout may differ.
    if (frame_header.version != kFrameVersion)
    {
        return false;
    }

    // Exact-length check: a frame shorter than expected means a truncated
    // receive; longer means a trailing-garbage or framing error.  Both are
    // rejected to prevent partial or over-read payloads.
    const std::size_t expected_frame_length =
        kEthernetFrameHeaderSize +
        static_cast<std::size_t>(frame_header.payload_length);

    if (frame_length != expected_frame_length)
    {
        return false;
    }

    transport_message.service_id = frame_header.service_id;
    transport_message.instance_id = frame_header.instance_id;
    transport_message.method_or_event_id = frame_header.method_or_event_id;
    transport_message.client_id = frame_header.client_id;
    transport_message.session_id = frame_header.session_id;
    transport_message.is_event =
        ((frame_header.flags & kFlagIsEventMask) != 0U);
    transport_message.is_reliable =
        ((frame_header.flags & kFlagIsReliableMask) != 0U);

    transport_message.payload.clear();

    if (frame_header.payload_length > 0U)
    {
        const std::uint8_t* const payload_start =
            frame_data + kEthernetFrameHeaderSize;

        transport_message.payload.resize(
            static_cast<std::size_t>(frame_header.payload_length));

        std::memcpy(
            transport_message.payload.data(),
            payload_start,
            static_cast<std::size_t>(frame_header.payload_length));
    }

    message_kind = frame_header.message_kind;
    return true;
}

bool DecodeTransportMessage(
    const std::vector<std::uint8_t>& frame_bytes,
    TransportMessage& transport_message,
    std::uint16_t& message_kind) noexcept
{
    return DecodeTransportMessage(
        frame_bytes.data(),
        frame_bytes.size(),
        transport_message,
        message_kind);
}

}  // namespace ethernet
}  // namespace transport
}  // namespace lighting
}  // namespace body_control