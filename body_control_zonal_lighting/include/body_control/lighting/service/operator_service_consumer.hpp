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
 * @brief Out-of-process proxy for the operator service.
 *
 * Used by HMI and diagnostic-console processes.  Sends lamp-command and
 * node-health requests to the controller's operator socket and receives
 * LampStatus / NodeHealth events back from it.  Maintains a local cache
 * so callers can read the last known state without polling.
 */
class OperatorServiceConsumer final
    : public OperatorServiceProviderInterface
    , public transport::TransportMessageHandlerInterface
{
public:
    OperatorServiceConsumer(
        transport::TransportAdapterInterface& transport,
        std::uint16_t client_id) noexcept;

    [[nodiscard]] OperatorServiceStatus Initialize() override;
    [[nodiscard]] OperatorServiceStatus Shutdown() override;

    [[nodiscard]] OperatorServiceStatus RequestLampToggle(
        domain::LampFunction lamp_function) override;

    [[nodiscard]] OperatorServiceStatus RequestLampActivate(
        domain::LampFunction lamp_function) override;

    [[nodiscard]] OperatorServiceStatus RequestLampDeactivate(
        domain::LampFunction lamp_function) override;

    [[nodiscard]] OperatorServiceStatus RequestNodeHealth() override;

    [[nodiscard]] bool GetLampStatus(
        domain::LampFunction lamp_function,
        domain::LampStatus& lamp_status) const noexcept override;

    void GetNodeHealthStatus(
        domain::NodeHealthStatus& node_health_status) const noexcept override;

    void SetEventListener(
        OperatorServiceEventListenerInterface* listener) noexcept override;

    // TransportMessageHandlerInterface — receives events from the controller.
    void OnTransportMessageReceived(
        const transport::TransportMessage& transport_message) override;

    void OnTransportAvailabilityChanged(
        bool is_available) override;

private:
    static constexpr std::size_t kMaxLampFunctions {5U};

    [[nodiscard]] OperatorServiceStatus SendRequest(
        const transport::TransportMessage& msg);

    static std::size_t LampFunctionToIndex(
        domain::LampFunction lamp_function) noexcept;

    static OperatorServiceStatus ConvertTransportStatus(
        transport::TransportStatus transport_status) noexcept;

    transport::TransportAdapterInterface& transport_;
    OperatorServiceEventListenerInterface* listener_;
    bool is_initialized_;
    std::uint16_t client_id_;
    std::uint16_t next_session_id_;

    std::array<domain::LampStatus, kMaxLampFunctions> cached_lamp_statuses_;
    domain::NodeHealthStatus cached_node_health_status_;
};

}  // namespace service
}  // namespace lighting
}  // namespace body_control

#endif  // BODY_CONTROL_LIGHTING_SERVICE_OPERATOR_SERVICE_CONSUMER_HPP_
