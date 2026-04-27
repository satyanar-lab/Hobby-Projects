#include "qml_hmi_bridge.hpp"

#include <QMetaObject>

#include "body_control/lighting/hmi/hmi_action_types.hpp"

using body_control::lighting::domain::LampFunction;
using body_control::lighting::domain::LampOutputState;
using body_control::lighting::domain::LampStatus;
using body_control::lighting::domain::NodeHealthStatus;
using body_control::lighting::hmi::HmiAction;
using body_control::lighting::hmi::MainWindow;
using body_control::lighting::hmi::MainWindowStatus;

QmlHmiBridge::QmlHmiBridge(MainWindow& main_window, QObject* parent)
    : QObject(parent)
    , main_window_(main_window)
{
}

void QmlHmiBridge::OnLampStatusUpdated(const LampStatus& lamp_status)
{
    const LampStatus snapshot = lamp_status;
    QMetaObject::invokeMethod(this, [this, snapshot]() {
        main_window_.OnLampStatusUpdated(snapshot);
        emit lampStatusChanged(static_cast<int>(snapshot.function));
    }, Qt::QueuedConnection);
}

void QmlHmiBridge::OnNodeHealthUpdated(const NodeHealthStatus& node_health_status)
{
    const NodeHealthStatus snapshot = node_health_status;
    QMetaObject::invokeMethod(this, [this, snapshot]() {
        main_window_.OnNodeHealthUpdated(snapshot);
        eth_up_        = snapshot.ethernet_link_available;
        svc_up_        = snapshot.service_available;
        fault_present_ = snapshot.lamp_driver_fault_present;
        fault_count_   = static_cast<int>(snapshot.active_fault_count);
        health_state_  = static_cast<int>(snapshot.health_state);
        emit nodeHealthChanged();
    }, Qt::QueuedConnection);
}

void QmlHmiBridge::OnControllerAvailabilityChanged(bool is_available)
{
    QMetaObject::invokeMethod(this, [this, is_available]() {
        main_window_.OnControllerAvailabilityChanged(is_available);
        controller_available_ = is_available;
        emit controllerAvailableChanged(is_available);
    }, Qt::QueuedConnection);
}

void QmlHmiBridge::toggleLeftIndicator()
{
    static_cast<void>(main_window_.ProcessAction(HmiAction::kToggleLeftIndicator));
}

void QmlHmiBridge::toggleRightIndicator()
{
    static_cast<void>(main_window_.ProcessAction(HmiAction::kToggleRightIndicator));
}

void QmlHmiBridge::toggleHazardLamp()
{
    static_cast<void>(main_window_.ProcessAction(HmiAction::kToggleHazardLamp));
}

void QmlHmiBridge::toggleParkLamp()
{
    static_cast<void>(main_window_.ProcessAction(HmiAction::kToggleParkLamp));
}

void QmlHmiBridge::toggleHeadLamp()
{
    static_cast<void>(main_window_.ProcessAction(HmiAction::kToggleHeadLamp));
}

void QmlHmiBridge::requestNodeHealth()
{
    static_cast<void>(main_window_.ProcessAction(HmiAction::kRequestNodeHealth));
}

bool QmlHmiBridge::isLampOutputOn(const int lamp_function) const noexcept
{
    LampStatus status {};
    const auto func = static_cast<LampFunction>(lamp_function);
    if (main_window_.GetViewModel().GetLampStatus(func, status))
    {
        return status.output_state == LampOutputState::kOn;
    }
    return false;
}

bool QmlHmiBridge::isLampCommandActive(const int lamp_function) const noexcept
{
    LampStatus status {};
    const auto func = static_cast<LampFunction>(lamp_function);
    if (main_window_.GetViewModel().GetLampStatus(func, status))
    {
        return status.command_applied;
    }
    return false;
}
