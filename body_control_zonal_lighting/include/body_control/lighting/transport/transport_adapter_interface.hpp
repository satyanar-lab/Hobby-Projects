#ifndef BODY_CONTROL_LIGHTING_TRANSPORT_TRANSPORT_ADAPTER_INTERFACE_HPP
#define BODY_CONTROL_LIGHTING_TRANSPORT_TRANSPORT_ADAPTER_INTERFACE_HPP

#include <cstdint>
#include <vector>

#include "body_control/lighting/transport/transport_status.hpp"

namespace body_control
{
namespace lighting
{
namespace transport
{

struct TransportMessage
{
    std::uint16_t service_id {0U};
    std::uint16_t instance_id {0U};
    std::uint16_t method_or_event_id {0U};
    std::uint16_t client_id {0U};
    std::uint16_t session_id {0U};
    bool is_event {false};
    bool is_reliable {true};
    std::vector<std::uint8_t> payload {};
};

class TransportMessageHandlerInterface
{
public:
    virtual ~TransportMessageHandlerInterface() = default;

    virtual void OnTransportMessageReceived(
        const TransportMessage& transport_message) = 0;

    virtual void OnTransportAvailabilityChanged(
        bool is_available) = 0;
};

class TransportAdapterInterface
{
public:
    virtual ~TransportAdapterInterface() = default;

    virtual TransportStatus Initialize() = 0;
    virtual TransportStatus Shutdown() = 0;

    virtual TransportStatus SendRequest(
        const TransportMessage& transport_message) = 0;

    virtual TransportStatus SendResponse(
        const TransportMessage& transport_message) = 0;

    virtual TransportStatus SendEvent(
        const TransportMessage& transport_message) = 0;

    virtual void SetMessageHandler(
        TransportMessageHandlerInterface* message_handler) noexcept = 0;
};

}  // namespace transport
}  // namespace lighting
}  // namespace body_control

#endif  // BODY_CONTROL_LIGHTING_TRANSPORT_TRANSPORT_ADAPTER_INTERFACE_HPP