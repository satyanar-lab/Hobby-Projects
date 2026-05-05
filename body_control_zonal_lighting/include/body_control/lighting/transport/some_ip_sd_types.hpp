#ifndef BODY_CONTROL_LIGHTING_TRANSPORT_SOME_IP_SD_TYPES_HPP
#define BODY_CONTROL_LIGHTING_TRANSPORT_SOME_IP_SD_TYPES_HPP

#include <cstdint>

namespace body_control
{
namespace lighting
{
namespace transport
{

enum class SdStatus : std::uint8_t
{
    kSuccess        = 0U,
    kSocketError    = 1U,
    kNotInitialized = 2U,
};

/// Parameters supplied by the service provider to advertise itself via SD.
struct ServiceOffer
{
    std::uint16_t service_id    {0U};
    std::uint16_t instance_id   {0U};
    std::uint8_t  major_version {1U};
    std::uint32_t minor_version {0U};
    std::uint32_t ttl_seconds   {5U};   ///< How long offer is valid after last receipt.
    char          local_ip[16U] {};     ///< Dotted-decimal unicast IP of this node.
    std::uint16_t local_port    {0U};   ///< UDP port for service data traffic.
    std::uint32_t period_ms     {2000U};///< OfferService transmission interval (main phase).
};

/// Populated by SomeIpSdListener when a valid OfferService is received.
struct DiscoveredService
{
    std::uint16_t service_id    {0U};
    std::uint16_t instance_id   {0U};
    std::uint8_t  major_version {0U};
    char          remote_ip[16U]{};     ///< Dotted-decimal unicast IP of the provider.
    std::uint16_t remote_port   {0U};   ///< UDP port for service data traffic.
};

}  // namespace transport
}  // namespace lighting
}  // namespace body_control

#endif  // BODY_CONTROL_LIGHTING_TRANSPORT_SOME_IP_SD_TYPES_HPP
