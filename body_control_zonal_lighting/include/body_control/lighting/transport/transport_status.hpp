#ifndef BODY_CONTROL_LIGHTING_TRANSPORT_TRANSPORT_STATUS_HPP_
#define BODY_CONTROL_LIGHTING_TRANSPORT_TRANSPORT_STATUS_HPP_

#include <cstdint>

namespace body_control::lighting::transport
{

/**
 * Return status for all transport layer send and receive operations.
 *
 * Upper layers map these values to their own status types (ServiceStatus,
 * OperatorServiceStatus) rather than leaking transport-level detail into
 * the service or application layers.
 */
enum class TransportStatus : std::uint8_t
{
    kSuccess            = 0U,  ///< Message sent or adapter initialised successfully.
    kNotInitialized     = 1U,  ///< Initialize() has not been called; socket not open.
    kInvalidArgument    = 2U,  ///< Message payload or address field is malformed.
    kServiceUnavailable = 3U,  ///< Remote endpoint is unreachable (service not offered).
    kTransmissionFailed = 4U,  ///< UDP send call returned an error.
    kReceptionFailed    = 5U,  ///< Inbound datagram could not be decoded or was truncated.
};

/**
 * Convenience predicate — returns true only when status is kSuccess.
 *
 * Avoids callers writing explicit comparisons against kSuccess in every
 * if-statement, and makes the intent clear when used as an assertion.
 */
constexpr bool IsTransportStatusSuccess(
    const TransportStatus transport_status) noexcept
{
    return (transport_status == TransportStatus::kSuccess);
}

}  // namespace body_control::lighting::transport

#endif  // BODY_CONTROL_LIGHTING_TRANSPORT_TRANSPORT_STATUS_HPP_
