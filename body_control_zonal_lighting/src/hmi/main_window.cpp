#include "body_control/lighting/hmi/main_window.hpp"

namespace body_control
{
namespace lighting
{
namespace hmi
{

MainWindow::MainWindow(
    service::OperatorServiceProviderInterface& operator_service) noexcept
    : operator_service_(operator_service)
    , hmi_view_model_ {}
{
}

MainWindowStatus MainWindow::ProcessAction(
    const HmiAction action)
{
    service::OperatorServiceStatus operator_status {
        service::OperatorServiceStatus::kInvalidArgument};

    switch (action)
    {
    case HmiAction::kToggleLeftIndicator:
        operator_status = operator_service_.RequestLampToggle(
            domain::LampFunction::kLeftIndicator);
        break;

    case HmiAction::kToggleRightIndicator:
        operator_status = operator_service_.RequestLampToggle(
            domain::LampFunction::kRightIndicator);
        break;

    case HmiAction::kToggleHazardLamp:
        operator_status = operator_service_.RequestLampToggle(
            domain::LampFunction::kHazardLamp);
        break;

    case HmiAction::kToggleParkLamp:
        operator_status = operator_service_.RequestLampToggle(
            domain::LampFunction::kParkLamp);
        break;

    case HmiAction::kToggleHeadLamp:
        operator_status = operator_service_.RequestLampToggle(
            domain::LampFunction::kHeadLamp);
        break;

    case HmiAction::kRequestNodeHealth:
        operator_status = operator_service_.RequestNodeHealth();
        break;

    case HmiAction::kUnknown:
    default:
        return MainWindowStatus::kInvalidAction;
    }

    return ConvertOperatorStatus(operator_status);
}

void MainWindow::OnLampStatusUpdated(
    const domain::LampStatus& lamp_status)
{
    hmi_view_model_.UpdateLampStatus(lamp_status);
}

void MainWindow::OnNodeHealthUpdated(
    const domain::NodeHealthStatus& node_health_status)
{
    hmi_view_model_.UpdateNodeHealthStatus(node_health_status);
}

void MainWindow::OnControllerAvailabilityChanged(
    const bool /*is_available*/)
{
    // Availability is surfaced via node health events; no separate
    // view model state defined for this phase.
}

const HmiViewModel& MainWindow::GetViewModel() const noexcept
{
    return hmi_view_model_;
}

MainWindowStatus MainWindow::ConvertOperatorStatus(
    const service::OperatorServiceStatus operator_status) noexcept
{
    MainWindowStatus main_window_status {MainWindowStatus::kControllerError};

    switch (operator_status)
    {
    case service::OperatorServiceStatus::kSuccess:
        main_window_status = MainWindowStatus::kSuccess;
        break;

    case service::OperatorServiceStatus::kInvalidArgument:
        main_window_status = MainWindowStatus::kInvalidAction;
        break;

    // kRejected (arbitration block), kNotAvailable (rear node unreachable),
    // kNotInitialized, and kTransportError are all surfaced as kControllerError;
    // the HMI has no actionable response for any of these states.
    case service::OperatorServiceStatus::kNotInitialized:
    case service::OperatorServiceStatus::kNotAvailable:
    case service::OperatorServiceStatus::kRejected:
    case service::OperatorServiceStatus::kTransportError:
    default:
        main_window_status = MainWindowStatus::kControllerError;
        break;
    }

    return main_window_status;
}

}  // namespace hmi
}  // namespace lighting
}  // namespace body_control
