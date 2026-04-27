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

/**
 * A SOME/IP-style message envelope passed between service and transport layers.
 *
 * Fields mirror the SOME/IP header fields used in this project.  The
 * transport adapter serialises these into on-wire datagrams; the service layer
 * fills or reads them without knowing the wire format.
 */
struct TransportMessage
{
    /// SOME/IP service identifier (e.g. kServiceId from lighting_service_ids.hpp).
    std::uint16_t service_id {0U};

    /// SOME/IP instance identifier; always 0x0001 in this single-node design.
    std::uint16_t instance_id {0U};

    /**
     * SOME/IP method ID for request/response messages, or event ID for push messages.
     * Event IDs have the MSB set (0x8xxx) per the SOME/IP specification.
     */
    std::uint16_t method_or_event_id {0U};

    /// SOME/IP client ID identifying the requesting application.
    std::uint16_t client_id {0U};

    /// SOME/IP session ID, incremented per request to correlate responses.
    std::uint16_t session_id {0U};

    /// True when this message carries an event notification (fire-and-forget).
    bool is_event {false};

    /// Reserved for future use; true indicates reliable delivery is preferred.
    bool is_reliable {true};

    /// Serialised domain payload (lamp command, lamp status, or health snapshot).
    std::vector<std::uint8_t> payload {};
};

/**
 * Callback interface implemented by whoever owns a TransportAdapterInterface.
 *
 * The transport adapter calls these methods when an inbound message arrives or
 * when the remote endpoint becomes available or unavailable.  Implementations
 * must not block — the adapter may call them from an interrupt or OS thread.
 */
class TransportMessageHandlerInterface
{
public:
    virtual ~TransportMessageHandlerInterface() = default;

    /**
     * Called for every successfully decoded inbound TransportMessage.
     *
     * @param transport_message  Decoded message; valid only for the duration of the call.
     */
    virtual void OnTransportMessageReceived(
        const TransportMessage& transport_message) = 0;

    /**
     * Called when the transport detects a link-state or service-availability change.
     *
     * @param is_available  true = remote endpoint is reachable; false = lost contact.
     */
    virtual void OnTransportAvailabilityChanged(
        bool is_available) = 0;
};

/**
 * Abstract transport layer used by service consumers and providers.
 *
 * Concrete implementations include the Linux vsomeip/UDP adapter and the
 * STM32 LwIP UDP adapter.  The service layer depends only on this interface,
 * keeping it platform-independent and testable with a mock transport.
 *
 * Three send methods are provided so callers can express their intent
 * (request, response, or event) even though the current UDP implementation
 * treats all three identically on the wire.  The distinction is preserved
 * for future reliable-transport support.
 */
class TransportAdapterInterface
{
public:
    virtual ~TransportAdapterInterface() = default;

    /**
     * Opens the socket, binds to the local port, and registers the message handler.
     *
     * @return kSuccess or kTransmissionFailed if the socket cannot be opened.
     */
    virtual TransportStatus Initialize() = 0;

    /**
     * Closes the socket and releases all network resources.
     *
     * @return kSuccess; never fails.
     */
    virtual TransportStatus Shutdown() = 0;

    /**
     * Sends a request message (method call expecting a response).
     *
     * @param transport_message  Must have is_event = false.
     * @return kSuccess, kNotInitialized, kInvalidArgument, or kTransmissionFailed.
     */
    virtual TransportStatus SendRequest(
        const TransportMessage& transport_message) = 0;

    /**
     * Sends a response message back to the original requester.
     *
     * The caller must echo the client_id and session_id from the request so
     * the remote side can correlate request to response.
     *
     * @param transport_message  Must carry the echoed client_id and session_id.
     */
    virtual TransportStatus SendResponse(
        const TransportMessage& transport_message) = 0;

    /**
     * Publishes an event notification to all subscribed clients.
     *
     * @param transport_message  Must have is_event = true and an event method_or_event_id.
     */
    virtual TransportStatus SendEvent(
        const TransportMessage& transport_message) = 0;

    /**
     * Registers the object that receives decoded inbound messages.
     * Pass nullptr to deregister.  Only one handler is supported.
     */
    virtual void SetMessageHandler(
        TransportMessageHandlerInterface* message_handler) noexcept = 0;
};

}  // namespace transport
}  // namespace lighting
}  // namespace body_control

#endif  // BODY_CONTROL_LIGHTING_TRANSPORT_TRANSPORT_ADAPTER_INTERFACE_HPP
