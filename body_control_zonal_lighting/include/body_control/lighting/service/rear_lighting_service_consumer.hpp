#ifndef BODY_CONTROL_LIGHTING_SERVICE_REAR_LIGHTING_SERVICE_CONSUMER_HPP_
#define BODY_CONTROL_LIGHTING_SERVICE_REAR_LIGHTING_SERVICE_CONSUMER_HPP_

#include <cstddef>
#include <cstdint>

#include "body_control/lighting/service/rear_lighting_service_interface.hpp"

namespace body_control::lighting::transport
{

class TransportAdapterInterface;

}  // namespace body_control::lighting::transport

namespace body_control::lighting::service
{

/**
 * @brief Concrete consumer-side service adapter used by the Central Zone
 *        Controller.
 */
class RearLightingServiceConsumer final : public RearLightingServiceConsumerInterface
{
public:
    explicit RearLightingServiceConsumer(
        transport::TransportAdapterInterface& transport_adapter) noexcept;

    RearLightingServiceConsumer(const RearLightingServiceConsumer&) = delete;
    RearLightingServiceConsumer& operator=(const RearLightingServiceConsumer&) = delete;
    RearLightingServiceConsumer(RearLightingServiceConsumer&&) = delete;
    RearLightingServiceConsumer& operator=(RearLightingServiceConsumer&&) = delete;

    ~RearLightingServiceConsumer() override = default;

    [[nodiscard]] ServiceStatus Initialize() override;
    [[nodiscard]] ServiceStatus Shutdown() override;

    [[nodiscard]] ServiceStatus SendLampCommand(
        const domain::LampCommand& command) override;

    [[nodiscard]] ServiceStatus RequestLampStatus(
        domain::LampFunction function) override;

    [[nodiscard]] ServiceStatus RequestNodeHealth() override;

    [[nodiscard]] bool IsServiceAvailable() const noexcept override;

    void SetEventListener(
        RearLightingServiceEventListenerInterface* event_listener) noexcept override;

    [[nodiscard]] ServiceStatus OnLampStatusPayloadReceived(
        const std::uint8_t* payload_data,
        std::size_t payload_length) noexcept;

    [[nodiscard]] ServiceStatus OnNodeHealthPayloadReceived(
        const std::uint8_t* payload_data,
        std::size_t payload_length) noexcept;

    void SetServiceAvailability(bool is_available) noexcept;

private:
    transport::TransportAdapterInterface& transport_adapter_;
    RearLightingServiceEventListenerInterface* event_listener_ {nullptr};
    bool is_initialized_ {false};
    bool is_service_available_ {false};
};

}  // namespace body_control::lighting::service

#endif  // BODY_CONTROL_LIGHTING_SERVICE_REAR_LIGHTING_SERVICE_CONSUMER_HPP_