#ifndef BODY_CONTROL_LIGHTING_TRANSPORT_TRANSPORT_STATUS_HPP_
#define BODY_CONTROL_LIGHTING_TRANSPORT_TRANSPORT_STATUS_HPP_

#include <cstdint>

namespace body_control::lighting::transport
{

/**
 * @brief Result status for transport-layer operations.
 */
enum class TransportStatus : std::uint8_t
{
    kSuccess = 0U,
    kNotInitialized = 1U,
    kInvalidArgument = 2U,
    kServiceUnavailable = 3U,
    kTransmissionFailed = 4U,
    kReceptionFailed = 5U
};

constexpr bool IsTransportStatusSuccess(
    const TransportStatus transport_status) noexcept
{
    return (transport_status == TransportStatus::kSuccess);
}

}  // namespace body_control::lighting::transport

#endif  // BODY_CONTROL_LIGHTING_TRANSPORT_TRANSPORT_STATUS_HPP_