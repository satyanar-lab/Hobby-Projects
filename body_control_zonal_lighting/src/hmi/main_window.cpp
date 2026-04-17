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
    domain::LampFunction toggled_function {domain::LampFunction::kUnknown};

    switch (action)
    {
    case HmiAction::kToggleLeftIndicator:
    {
        toggled_function = domain::LampFunction::kLeftIndicator;
        controller_status = central_zone_controller_.SendLampCommand(
            toggled_function,
            hmi_view_model_.IsLampFunctionActive(toggled_function)
                ? domain::LampCommandAction::kDeactivate
                : domain::LampCommandAction::kActivate,
            domain::CommandSource::kHmiControlPanel);
        break;
    }

    case HmiAction::kToggleRightIndicator:
    {
        toggled_function = domain::LampFunction::kRightIndicator;
        controller_status = central_zone_controller_.SendLampCommand(
            toggled_function,
            hmi_view_model_.IsLampFunctionActive(toggled_function)
                ? domain::LampCommandAction::kDeactivate
                : domain::LampCommandAction::kActivate,
            domain::CommandSource::kHmiControlPanel);
        break;
    }

    case HmiAction::kToggleHazardLamp:
    {
        toggled_function = domain::LampFunction::kHazardLamp;
        controller_status = central_zone_controller_.SendLampCommand(
            toggled_function,
            hmi_view_model_.IsLampFunctionActive(toggled_function)
                ? domain::LampCommandAction::kDeactivate
                : domain::LampCommandAction::kActivate,
            domain::CommandSource::kHmiControlPanel);
        break;
    }

    case HmiAction::kToggleParkLamp:
    {
        toggled_function = domain::LampFunction::kParkLamp;
        controller_status = central_zone_controller_.SendLampCommand(
            toggled_function,
            hmi_view_model_.IsLampFunctionActive(toggled_function)
                ? domain::LampCommandAction::kDeactivate
                : domain::LampCommandAction::kActivate,
            domain::CommandSource::kHmiControlPanel);
        break;
    }

    case HmiAction::kToggleHeadLamp:
    {
        toggled_function = domain::LampFunction::kHeadLamp;
        controller_status = central_zone_controller_.SendLampCommand(
            toggled_function,
            hmi_view_model_.IsLampFunctionActive(toggled_function)
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

    // For lamp toggles: pull confirmed state from the node so the view
    // model is up-to-date before the caller redraws.  The status request
    // is best-effort; the view model is refreshed regardless.
    if (toggled_function != domain::LampFunction::kUnknown)
    {
        static_cast<void>(
            central_zone_controller_.RequestLampStatus(toggled_function));

        domain::LampStatus refreshed_status {};
        if (central_zone_controller_.GetCachedLampStatus(
                toggled_function, refreshed_status))
        {
            hmi_view_model_.UpdateLampStatus(refreshed_status);
        }
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