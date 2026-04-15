#ifndef BODY_CONTROL_LIGHTING_SERVICE_REAR_LIGHTING_SERVICE_CONSUMER_HPP
#define BODY_CONTROL_LIGHTING_SERVICE_REAR_LIGHTING_SERVICE_CONSUMER_HPP

#include <cstdint>

#include "body_control/lighting/service/rear_lighting_service_interface.hpp"
#include "body_control/lighting/transport/transport_adapter_interface.hpp"

namespace body_control
{
namespace lighting
{
namespace service
{

class RearLightingServiceConsumer final
    : public RearLightingServiceConsumerInterface
    , public transport::TransportMessageHandlerInterface
{
public:
    explicit RearLightingServiceConsumer(
        transport::TransportAdapterInterface& transport_adapter) noexcept;

    ServiceStatus Initialize() override;
    ServiceStatus Shutdown() override;

    ServiceStatus SendLampCommand(
        const domain::LampCommand& lamp_command) override;

    ServiceStatus RequestLampStatus(
        domain::LampFunction lamp_function) override;

    ServiceStatus RequestNodeHealth() override;

    void SetEventListener(
        RearLightingServiceEventListenerInterface* event_listener) noexcept override;

    bool IsServiceAvailable() const noexcept override;

    void OnTransportMessageReceived(
        const transport::TransportMessage& transport_message) override;

    void OnTransportAvailabilityChanged(
        bool is_available) override;

private:
    void OnLampStatusPayloadReceived(
        const transport::TransportMessage& transport_message);

    void OnNodeHealthPayloadReceived(
        const transport::TransportMessage& transport_message);

    void SetServiceAvailability(
        bool is_service_available) noexcept;

    static ServiceStatus ConvertTransportStatus(
        transport::TransportStatus transport_status) noexcept;

    transport::TransportAdapterInterface& transport_adapter_;
    RearLightingServiceEventListenerInterface* event_listener_;
    bool is_initialized_;
    bool is_service_available_;
    std::uint16_t client_id_;
    std::uint16_t next_session_id_;
};

}  // namespace service
}  // namespace lighting
}  // namespace body_control

#endif  // BODY_CONTROL_LIGHTING_SERVICE_REAR_LIGHTING_SERVICE_CONSUMER_HPP