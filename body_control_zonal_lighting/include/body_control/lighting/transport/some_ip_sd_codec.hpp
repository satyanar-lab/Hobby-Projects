#ifndef BODY_CONTROL_LIGHTING_TRANSPORT_SOME_IP_SD_CODEC_HPP
#define BODY_CONTROL_LIGHTING_TRANSPORT_SOME_IP_SD_CODEC_HPP

#include <cstddef>
#include <cstdint>
#include <vector>

#include "body_control/lighting/transport/some_ip_sd_types.hpp"

namespace body_control
{
namespace lighting
{
namespace transport
{
namespace SomeIpSdCodec
{

// Encode a SOME/IP-SD OfferService frame per AUTOSAR AP_PRS_SOMEIPServiceDiscovery.
//
// Wire format: standard 16-byte SOME/IP header (Service ID 0xFFFF, Method ID 0x8100)
// followed by 40 bytes of SD payload (flags, entries array, options array with one
// IPv4 Endpoint Option).  Total: 56 bytes.
//
// This is the ONLY place in the project that uses the standard 16-byte SOME/IP header;
// all service-data messages use the project-specific 20-byte custom header instead.
// Rationale: Wireshark's SOME/IP-SD dissector requires the standard header format.
[[nodiscard]] std::vector<std::uint8_t> EncodeOffer(
    const ServiceOffer& offer,
    std::uint16_t       session_id) noexcept;

// Encode a SOME/IP-SD FindService frame (no endpoint option).  Total: 44 bytes.
[[nodiscard]] std::vector<std::uint8_t> EncodeFind(
    std::uint16_t service_id,
    std::uint16_t instance_id,
    std::uint16_t session_id) noexcept;

// Parse an incoming SD frame.
// Returns true if the frame contains an OfferService entry for the requested
// service_id and instance_id.  On true, out is populated with the provider's
// unicast endpoint from the IPv4 Endpoint Option.
[[nodiscard]] bool DecodeOffer(
    const std::uint8_t* data,
    std::size_t         len,
    std::uint16_t       wanted_service_id,
    std::uint16_t       wanted_instance_id,
    DiscoveredService&  out) noexcept;

}  // namespace SomeIpSdCodec
}  // namespace transport
}  // namespace lighting
}  // namespace body_control

#endif  // BODY_CONTROL_LIGHTING_TRANSPORT_SOME_IP_SD_CODEC_HPP
