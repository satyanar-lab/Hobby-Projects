#ifndef BODY_CONTROL_LIGHTING_SERVICE_OPERATOR_SERVICE_CONSUMER_HPP_
#define BODY_CONTROL_LIGHTING_SERVICE_OPERATOR_SERVICE_CONSUMER_HPP_

#include <array>
#include <cstdint>

#include "body_control/lighting/service/operator_service_interface.hpp"
#include "body_control/lighting/transport/transport_adapter_interface.hpp"

namespace body_control
{
namespace lighting
{
namespace service
{

/**
 * Out-of-process proxy for the operator service, used by the HMI and
 * diagnostic-console processes.
 *
 * Translates each Request* call into a UDP datagram addressed to the
 * controller's operator port, and demultiplexes incoming datagrams from
 * the controller into OnLampStatusUpdated / OnNodeHealthUpdated callbacks
 * on the registered event listener.
 *
 * Maintains a local lamp-status and node-health cache so the HMI can read
 * the last known state synchronously without waiting for a network round-trip.
 * The cache is updated on every event push from the controller.
 *
 * This class also implements TransportMessageHandlerInterface so that the
 * transport adapter calls directly back into it when a datagram arrives —
 * no external dispatcher is needed.
 */
class OperatorServiceConsumer final
    : public OperatorServiceProviderInterface
    , public transport::TransportMessageHandlerInterface
{
public:
    /**
     * @param transport   Transport adapter for the operator UDP channel.
     * @param client_id   SOME/IP client ID embedded in outbound request headers.
     */
    OperatorServiceConsumer(
        transport::TransportAdapterInterface& transport,
        std::uint16_t client_id) noexcept;

    /** Starts the transport and registers this consumer as the message handler. */
    [[nodiscard]] OperatorServiceStatus Initialize() override;

    /** Stops the transport and clears the event listener. */
    [[nodiscard]] OperatorServiceStatus Shutdown() override;

    /** Sends a RequestLampToggle datagram to the controller. */
    [[nodiscard]] OperatorServiceStatus RequestLampToggle(
        domain::LampFunction lamp_function) override;

    /** Sends a RequestLampActivate datagram to the controller. */
    [[nodiscard]] OperatorServiceStatus RequestLampActivate(
        domain::LampFunction lamp_function) override;

    /** Sends a RequestLampDeactivate datagram to the controller. */
    [[nodiscard]] OperatorServiceStatus RequestLampDeactivate(
        domain::LampFunction lamp_function) override;

    /** Sends a RequestNodeHealth datagram to the controller. */
    [[nodiscard]] OperatorServiceStatus RequestNodeHealth() override;

    /** Sends a RequestInjectFault datagram to the controller. */
    [[nodiscard]] OperatorServiceStatus RequestInjectFault(
        domain::LampFunction lamp_function) override;

    /** Sends a RequestClearFault datagram to the controller. */
    [[nodiscard]] OperatorServiceStatus RequestClearFault(
        domain::LampFunction lamp_function) override;

    /** Sends a RequestGetFaultStatus datagram to the controller. */
    [[nodiscard]] OperatorServiceStatus RequestGetFaultStatus() override;

    /** Reads the cached LampStatus for the given function. */
    [[nodiscard]] bool GetLampStatus(
        domain::LampFunction lamp_function,
        domain::LampStatus& lamp_status) const noexcept override;

    /** Reads the cached NodeHealthStatus. */
    void GetNodeHealthStatus(
        domain::NodeHealthStatus& node_health_status) const noexcept override;

    /** Registers the listener for asynchronous event callbacks. */
    void SetEventListener(
        OperatorServiceEventListenerInterface* listener) noexcept override;

    /**
     * Demultiplexes an inbound datagram from the controller.
     *
     * Routes to lamp-status or node-health handling based on the SOME/IP
     * event ID; updates the local cache and fires the registered listener.
     */
    void OnTransportMessageReceived(
        const transport::TransportMessage& transport_message) override;

    /** Notifies the event listener of controller availability changes. */
    void OnTransportAvailabilityChanged(
        bool is_available) override;

private:
    /// Capacity of the lamp-status cache — one slot per lamp function.
    static constexpr std::size_t kMaxLampFunctions {5U};

    /**
     * Serialises msg and sends it over the transport.
     *
     * Central send path used by all Request* methods; returns the mapped
     * OperatorServiceStatus on failure so callers do not duplicate conversion logic.
     */
    [[nodiscard]] OperatorServiceStatus SendRequest(
        const transport::TransportMessage& msg);

    /** Maps a LampFunction to a zero-based cache array index (0–4). */
    static std::size_t LampFunctionToIndex(
        domain::LampFunction lamp_function) noexcept;

    /** Maps a TransportStatus to the equivalent OperatorServiceStatus. */
    static OperatorServiceStatus ConvertTransportStatus(
        transport::TransportStatus transport_status) noexcept;

    transport::TransportAdapterInterface& transport_;

    /// Non-owning pointer; may be nullptr if no listener is registered.
    OperatorServiceEventListenerInterface* listener_;

    bool is_initialized_;

    /// Identifies this client in SOME/IP headers sent to the controller.
    std::uint16_t client_id_;

    /// Monotonically increasing SOME/IP session ID, incremented per request.
    std::uint16_t next_session_id_;

    /// Per-function cache; indexed by LampFunctionToIndex().
    std::array<domain::LampStatus, kMaxLampFunctions> cached_lamp_statuses_;

    /// Cached health snapshot; updated on every NodeHealth event from the controller.
    domain::NodeHealthStatus cached_node_health_status_;
};

}  // namespace service
}  // namespace lighting
}  // namespace body_control

#endif  // BODY_CONTROL_LIGHTING_SERVICE_OPERATOR_SERVICE_CONSUMER_HPP_
