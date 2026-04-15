#include "body_control/lighting/hmi/main_window.hpp"

#include "body_control/lighting/application/central_zone_controller.hpp"

namespace body_control::lighting::hmi
{

MainWindow::MainWindow(
    application::CentralZoneController& central_zone_controller) noexcept
    : central_zone_controller_(central_zone_controller)
{
}

MainWindowStatus MainWindow::ProcessAction(const HmiAction action)
{
    MainWindowStatus window_status {MainWindowStatus::kInvalidAction};

    auto convert_controller_status =
        [](const application::ControllerStatus controller_status) noexcept
        -> MainWindowStatus
    {
        if (controller_status == application::ControllerStatus::kSuccess)
        {
            return MainWindowStatus::kSuccess;
        }

        return MainWindowStatus::kInvalidAction;
    };

    switch (action)
    {
    case HmiAction::kToggleLeftIndicator:
    case HmiAction::kToggleRightIndicator:
    case HmiAction::kToggleHazardLamp:
    case HmiAction::kToggleParkLamp:
    case HmiAction::kToggleHeadLamp:
    {
        domain::LampFunction lamp_function {domain::LampFunction::kUnknown};

        switch (action)
        {
        case HmiAction::kToggleLeftIndicator:
            lamp_function = domain::LampFunction::kLeftIndicator;
            break;

        case HmiAction::kToggleRightIndicator:
            lamp_function = domain::LampFunction::kRightIndicator;
            break;

        case HmiAction::kToggleHazardLamp:
            lamp_function = domain::LampFunction::kHazardLamp;
            break;

        case HmiAction::kToggleParkLamp:
            lamp_function = domain::LampFunction::kParkLamp;
            break;

        case HmiAction::kToggleHeadLamp:
            lamp_function = domain::LampFunction::kHeadLamp;
            break;

        case HmiAction::kRequestNodeHealth:
        case HmiAction::kUnknown:
        default:
            lamp_function = domain::LampFunction::kUnknown;
            break;
        }

        const bool is_currently_active =
            hmi_view_model_.IsLampFunctionActive(lamp_function);

        const domain::LampCommandAction command_action =
            is_currently_active
                ? domain::LampCommandAction::kDeactivate
                : domain::LampCommandAction::kActivate;

        const application::ControllerStatus controller_status =
            central_zone_controller_.SendLampCommand(
                lamp_function,
                command_action,
                static_cast<domain::CommandSource>(0U));

        window_status = convert_controller_status(controller_status);
        break;
    }

    case HmiAction::kRequestNodeHealth:
    {
        const application::ControllerStatus controller_status =
            central_zone_controller_.RequestNodeHealth();

        window_status = convert_controller_status(controller_status);
        break;
    }

    case HmiAction::kUnknown:
    default:
        window_status = MainWindowStatus::kInvalidAction;
        break;
    }

    return window_status;
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

}  // namespace body_control::lighting::hmi