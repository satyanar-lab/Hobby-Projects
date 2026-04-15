#ifndef BODY_CONTROL_LIGHTING_APPLICATION_CENTRAL_ZONE_CONTROLLER_HPP_
#define BODY_CONTROL_LIGHTING_APPLICATION_CENTRAL_ZONE_CONTROLLER_HPP_

#include <cstdint>

#include "body_control/lighting/domain/lamp_command_types.hpp"
#include "body_control/lighting/domain/lamp_status_types.hpp"

namespace body_control::lighting::application
{

class LampStateManager;
class NodeHealthMonitor;

}  // namespace body_control::lighting::application

namespace body_control::lighting::service
{

class RearLightingServiceConsumer;

}  // namespace body_control::lighting::service

namespace body_control::lighting::application
{

/**
 * @brief Result status for Central Zone Controller operations.
 */
enum class ControllerStatus : std::uint8_t
{
    kSuccess = 0U,
    kNotInitialized = 1U,
    kInvalidArgument = 2U,
    kServiceUnavailable = 3U,
    kTransmissionFailed = 4U
};

/**
 * @brief Central application component responsible for issuing commands to the
 *        Rear Lighting Node and caching the returned status information.
 */
class CentralZoneController
{
public:
    CentralZoneController(
        service::RearLightingServiceConsumer& rear_lighting_service_consumer,
        LampStateManager& lamp_state_manager,
        NodeHealthMonitor& node_health_monitor) noexcept;

    CentralZoneController(const CentralZoneController&) = delete;
    CentralZoneController& operator=(const CentralZoneController&) = delete;
    CentralZoneController(CentralZoneController&&) = delete;
    CentralZoneController& operator=(CentralZoneController&&) = delete;

    ~CentralZoneController() = default;

    [[nodiscard]] ControllerStatus Initialize();
    [[nodiscard]] ControllerStatus Shutdown();

    [[nodiscard]] ControllerStatus SendLampCommand(
        domain::LampFunction function,
        domain::LampCommandAction action,
        domain::CommandSource source);

    [[nodiscard]] ControllerStatus RequestLampStatus(
        domain::LampFunction function);

    [[nodiscard]] ControllerStatus RequestNodeHealth();

    void OnLampStatusReceived(const domain::LampStatus& lamp_status);
    void OnNodeHealthReceived(const domain::NodeHealthStatus& node_health_status);

    [[nodiscard]] bool IsInitialized() const noexcept;

private:
    [[nodiscard]] std::uint16_t GetNextSequenceCounter() noexcept;

    service::RearLightingServiceConsumer& rear_lighting_service_consumer_;
    LampStateManager& lamp_state_manager_;
    NodeHealthMonitor& node_health_monitor_;

    std::uint16_t next_sequence_counter_ {1U};
    bool is_initialized_ {false};
};

}  // namespace body_control::lighting::application

#endif  // BODY_CONTROL_LIGHTING_APPLICATION_CENTRAL_ZONE_CONTROLLER_HPP_