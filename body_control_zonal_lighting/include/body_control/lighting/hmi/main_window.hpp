#ifndef BODY_CONTROL_LIGHTING_HMI_MAIN_WINDOW_HPP
#define BODY_CONTROL_LIGHTING_HMI_MAIN_WINDOW_HPP

#include "body_control/lighting/application/central_zone_controller.hpp"
#include "body_control/lighting/domain/lamp_status_types.hpp"
#include "body_control/lighting/hmi/hmi_command_mapper.hpp"
#include "body_control/lighting/hmi/hmi_view_model.hpp"

namespace body_control
{
namespace lighting
{
namespace hmi
{

enum class MainWindowStatus : std::uint8_t
{
    kSuccess = 0U,
    kInvalidAction = 1U,
    kControllerError = 2U
};

class MainWindow final
{
public:
    explicit MainWindow(
        application::CentralZoneController& central_zone_controller) noexcept;

    MainWindowStatus ProcessAction(
        HmiAction action);

    void UpdateLampStatus(
        const domain::LampStatus& lamp_status) noexcept;

    void UpdateNodeHealthStatus(
        const domain::NodeHealthStatus& node_health_status) noexcept;

    const HmiViewModel& GetViewModel() const noexcept;

private:
    static MainWindowStatus ConvertControllerStatus(
        application::ControllerStatus controller_status) noexcept;

    application::CentralZoneController& central_zone_controller_;
    HmiViewModel hmi_view_model_;
};

}  // namespace hmi
}  // namespace lighting
}  // namespace body_control

#endif  // BODY_CONTROL_LIGHTING_HMI_MAIN_WINDOW_HPP