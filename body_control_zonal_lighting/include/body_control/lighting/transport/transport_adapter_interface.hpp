#ifndef BODY_CONTROL_LIGHTING_TRANSPORT_TRANSPORT_ADAPTER_INTERFACE_HPP_
#define BODY_CONTROL_LIGHTING_TRANSPORT_TRANSPORT_ADAPTER_INTERFACE_HPP_

#include <cstddef>
#include <cstdint>

#include "body_control/lighting/transport/transport_status.hpp"

namespace body_control::lighting::transport
{

/**
 * @brief Generic transport message container.
 */
struct TransportMessage
{
    std::uint16_t service_id {0U};
    std::uint16_t instance_id {0U};
    std::uint16_t method_or_event_id {0U};
    const std::uint8_t* payload_data {nullptr};
    std::size_t payload_length {0U};
};

/**
 * @brief Callback interface for transport-level message delivery.
 */
class TransportMessageHandlerInterface
{
public:
    virtual ~TransportMessageHandlerInterface() = default;

    virtual void OnRequestReceived(
        const TransportMessage& message) = 0;

    virtual void OnResponseReceived(
        const TransportMessage& message) = 0;

    virtual void OnEventReceived(
        const TransportMessage& message) = 0;

    virtual void OnServiceAvailabilityChanged(
        std::uint16_t service_id,
        std::uint16_t instance_id,
        bool is_available) = 0;
};

/**
 * @brief Abstract transport adapter interface.
 *
 * This hides the underlying transport stack from the service layer.
 */
class TransportAdapterInterface
{
public:
    virtual ~TransportAdapterInterface() = default;

    [[nodiscard]] virtual TransportStatus Initialize() = 0;
    [[nodiscard]] virtual TransportStatus Shutdown() = 0;

    [[nodiscard]] virtual TransportStatus OfferService(
        std::uint16_t service_id,
        std::uint16_t instance_id) = 0;

    [[nodiscard]] virtual TransportStatus StopOfferService(
        std::uint16_t service_id,
        std::uint16_t instance_id) = 0;

    [[nodiscard]] virtual TransportStatus SendRequest(
        const TransportMessage& message) = 0;

    [[nodiscard]] virtual TransportStatus SendResponse(
        const TransportMessage& message) = 0;

    [[nodiscard]] virtual TransportStatus PublishEvent(
        const TransportMessage& message) = 0;

    [[nodiscard]] virtual bool IsServiceAvailable(
        std::uint16_t service_id,
        std::uint16_t instance_id) const noexcept = 0;

    virtual void SetMessageHandler(
        TransportMessageHandlerInterface* message_handler) noexcept = 0;
};

}  // namespace body_control::lighting::transport

#endif  // BODY_CONTROL_LIGHTING_TRANSPORT_TRANSPORT_ADAPTER_INTERFACE_HPP_