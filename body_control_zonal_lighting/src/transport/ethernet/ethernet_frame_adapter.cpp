#include "body_control/lighting/transport/ethernet/ethernet_frame_adapter.hpp"

namespace body_control::lighting::transport::ethernet
{
namespace
{

constexpr std::size_t kEthernetFrameHeaderLength {8U};

void WriteUint16BigEndian(
    const std::uint16_t value,
    std::uint8_t* destination) noexcept
{
    destination[0] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
    destination[1] = static_cast<std::uint8_t>(value & 0xFFU);
}

[[nodiscard]] std::uint16_t ReadUint16BigEndian(
    const std::uint8_t* source) noexcept
{
    return static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(source[0]) << 8U) |
        static_cast<std::uint16_t>(source[1]));
}

}  // namespace

EthernetFrameAdapterStatus EthernetFrameAdapter::BuildFrame(
    const TransportMessage& transport_message,
    EthernetFrame& ethernet_frame) const noexcept
{
    if ((transport_message.payload_length > 0U) &&
        (transport_message.payload_data == nullptr))
    {
        return EthernetFrameAdapterStatus::kInvalidArgument;
    }

    if ((kEthernetFrameHeaderLength + transport_message.payload_length) >
        ethernet_frame.frame_data.size())
    {
        return EthernetFrameAdapterStatus::kFrameTooLarge;
    }

    ethernet_frame.frame_data.fill(0U);

    WriteUint16BigEndian(transport_message.service_id, &ethernet_frame.frame_data[0]);
    WriteUint16BigEndian(transport_message.instance_id, &ethernet_frame.frame_data[2]);
    WriteUint16BigEndian(transport_message.message_id, &ethernet_frame.frame_data[4]);
    WriteUint16BigEndian(
        static_cast<std::uint16_t>(transport_message.payload_length),
        &ethernet_frame.frame_data[6]);

    for (std::size_t index {0U}; index < transport_message.payload_length; ++index)
    {
        ethernet_frame.frame_data[kEthernetFrameHeaderLength + index] =
            transport_message.payload_data[index];
    }

    ethernet_frame.frame_length =
        kEthernetFrameHeaderLength + transport_message.payload_length;

    return EthernetFrameAdapterStatus::kSuccess;
}

EthernetFrameAdapterStatus EthernetFrameAdapter::ParseFrame(
    const EthernetFrame& ethernet_frame,
    TransportMessage& transport_message) const noexcept
{
    if (ethernet_frame.frame_length < kEthernetFrameHeaderLength)
    {
        return EthernetFrameAdapterStatus::kInvalidFrame;
    }

    const std::uint16_t payload_length =
        ReadUint16BigEndian(&ethernet_frame.frame_data[6]);

    if ((kEthernetFrameHeaderLength + payload_length) > ethernet_frame.frame_length)
    {
        return EthernetFrameAdapterStatus::kInvalidFrame;
    }

    transport_message.service_id =
        ReadUint16BigEndian(&ethernet_frame.frame_data[0]);
    transport_message.instance_id =
        ReadUint16BigEndian(&ethernet_frame.frame_data[2]);
    transport_message.message_id =
        ReadUint16BigEndian(&ethernet_frame.frame_data[4]);
    transport_message.payload_length = payload_length;
    transport_message.payload_data =
        (payload_length > 0U)
            ? &ethernet_frame.frame_data[kEthernetFrameHeaderLength]
            : nullptr;

    return EthernetFrameAdapterStatus::kSuccess;
}

}  // namespace body_control::lighting::transport::ethernet