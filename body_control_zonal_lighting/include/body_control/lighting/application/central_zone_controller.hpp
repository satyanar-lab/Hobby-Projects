#ifndef BODY_CONTROL_LIGHTING_APPLICATION_CENTRAL_ZONE_CONTROLLER_HPP
#define BODY_CONTROL_LIGHTING_APPLICATION_CENTRAL_ZONE_CONTROLLER_HPP

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <thread>

#include "body_control/lighting/domain/lamp_command_types.hpp"
#include "body_control/lighting/domain/lamp_status_types.hpp"
#include "body_control/lighting/service/rear_lighting_service_interface.hpp"

namespace body_control
{
namespace lighting
{
namespace application
{

enum class ControllerStatus : std::uint8_t
{
    kSuccess = 0U,
    kNotInitialized = 1U,
    kNotAvailable = 2U,
    kInvalidArgument = 3U,
    kServiceError = 4U
};

class CentralZoneController final
    : public service::RearLightingServiceEventListenerInterface
{
public:
    explicit CentralZoneController(
        service::RearLightingServiceConsumerInterface& rear_lighting_service_consumer) noexcept;

    ControllerStatus Initialize();
    ControllerStatus Shutdown();

    ControllerStatus SendLampCommand(
        domain::LampFunction lamp_function,
        domain::LampCommandAction lamp_command_action,
        domain::CommandSource command_source);

    ControllerStatus RequestLampStatus(
        domain::LampFunction lamp_function);

    ControllerStatus RequestNodeHealth();

    bool GetCachedLampStatus(
        domain::LampFunction lamp_function,
        domain::LampStatus& lamp_status) const noexcept;

    domain::NodeHealthStatus GetCachedNodeHealthStatus() const noexcept;

    bool IsRearNodeAvailable() const noexcept;

    void OnLampStatusReceived(
        const domain::LampStatus& lamp_status) override;

    void OnNodeHealthStatusReceived(
        const domain::NodeHealthStatus& node_health_status) override;

    void OnServiceAvailabilityChanged(
        bool is_service_available) override;

private:
    static ControllerStatus ConvertServiceStatus(
        service::ServiceStatus service_status) noexcept;

    static std::size_t LampFunctionToIndex(
        domain::LampFunction lamp_function) noexcept;

    void RunHealthPollLoop();

    service::RearLightingServiceConsumerInterface& rear_lighting_service_consumer_;
    bool is_initialized_;
    bool is_rear_node_available_;
    std::uint16_t next_sequence_counter_;

    mutable std::mutex cache_mutex_;
    std::array<domain::LampStatus, 5U> cached_lamp_statuses_;
    domain::NodeHealthStatus cached_node_health_status_;

    std::atomic<bool> health_poll_active_;
    std::mutex health_poll_mutex_;
    std::condition_variable health_poll_cv_;
    std::thread health_poll_thread_;
};

}  // namespace application
}  // namespace lighting
}  // namespace body_control

#endif  // BODY_CONTROL_LIGHTING_APPLICATION_CENTRAL_ZONE_CONTROLLER_HPP