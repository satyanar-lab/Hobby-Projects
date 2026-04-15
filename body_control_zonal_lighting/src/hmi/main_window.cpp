#include "body_control/lighting/hmi/main_window.hpp"

namespace body_control
{
namespace lighting
{
namespace hmi
{

MainWindow::MainWindow(
    application::CentralZoneController& central_zone_controller) noexcept
    : central_zone_controller_(central_zone_controller)
    , hmi_view_model_ {}
{
}

MainWindowStatus MainWindow::ProcessAction(
    const HmiAction action)
{
    application::ControllerStatus controller_status {
        application::ControllerStatus::kInvalidArgument};

    switch (action)
    {
    case HmiAction::kToggleLeftIndicator:
    {
        controller_status = central_zone_controller_.SendLampCommand(
            domain::LampFunction::kLeftIndicator,
            hmi_view_model_.IsLampFunctionActive(
                domain::LampFunction::kLeftIndicator)
                ? domain::LampCommandAction::kDeactivate
                : domain::LampCommandAction::kActivate,
            domain::CommandSource::kHmiControlPanel);
        break;
    }

    case HmiAction::kToggleRightIndicator:
    {
        controller_status = central_zone_controller_.SendLampCommand(
            domain::LampFunction::kRightIndicator,
            hmi_view_model_.IsLampFunctionActive(
                domain::LampFunction::kRightIndicator)
                ? domain::LampCommandAction::kDeactivate
                : domain::LampCommandAction::kActivate,
            domain::CommandSource::kHmiControlPanel);
        break;
    }

    case HmiAction::kToggleHazardLamp:
    {
        controller_status = central_zone_controller_.SendLampCommand(
            domain::LampFunction::kHazardLamp,
            hmi_view_model_.IsLampFunctionActive(
                domain::LampFunction::kHazardLamp)
                ? domain::LampCommandAction::kDeactivate
                : domain::LampCommandAction::kActivate,
            domain::CommandSource::kHmiControlPanel);
        break;
    }

    case HmiAction::kToggleParkLamp:
    {
        controller_status = central_zone_controller_.SendLampCommand(
            domain::LampFunction::kParkLamp,
            hmi_view_model_.IsLampFunctionActive(
                domain::LampFunction::kParkLamp)
                ? domain::LampCommandAction::kDeactivate
                : domain::LampCommandAction::kActivate,
            domain::CommandSource::kHmiControlPanel);
        break;
    }

    case HmiAction::kToggleHeadLamp:
    {
        controller_status = central_zone_controller_.SendLampCommand(
            domain::LampFunction::kHeadLamp,
            hmi_view_model_.IsLampFunctionActive(
                domain::LampFunction::kHeadLamp)
                ? domain::LampCommandAction::kDeactivate
                : domain::LampCommandAction::kActivate,
            domain::CommandSource::kHmiControlPanel);
        break;
    }

    case HmiAction::kRequestNodeHealth:
    {
        controller_status = central_zone_controller_.RequestNodeHealth();
        break;
    }

    case HmiAction::kUnknown:
    default:
        return MainWindowStatus::kInvalidAction;
    }

    return ConvertControllerStatus(controller_status);
}

void MainWindow::UpdateLampStatus(
    const domain::LampStatus& lamp_status) noexcept
{
    hmi_view_model_.UpdateLampStatus(lamp_status);
}

void MainWindow::UpdateNodeHealthStatus(
    const domain::NodeHealthStatus& node_health_status) noexcept
{
    hmi_view_model_.UpdateNodeHealthStatus(node_health_status);
}

const HmiViewModel& MainWindow::GetViewModel() const noexcept
{
    return hmi_view_model_;
}

MainWindowStatus MainWindow::ConvertControllerStatus(
    const application::ControllerStatus controller_status) noexcept
{
    MainWindowStatus main_window_status {MainWindowStatus::kControllerError};

    switch (controller_status)
    {
    case application::ControllerStatus::kSuccess:
        main_window_status = MainWindowStatus::kSuccess;
        break;

    case application::ControllerStatus::kInvalidArgument:
        main_window_status = MainWindowStatus::kInvalidAction;
        break;

    case application::ControllerStatus::kNotInitialized:
    case application::ControllerStatus::kNotAvailable:
    case application::ControllerStatus::kServiceError:
    default:
        main_window_status = MainWindowStatus::kControllerError;
        break;
    }

    return main_window_status;
}

}  // namespace hmi
}  // namespace lighting
}  // namespace body_control