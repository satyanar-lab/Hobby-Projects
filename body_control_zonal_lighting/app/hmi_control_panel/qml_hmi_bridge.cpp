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

// The On* callbacks arrive on the vsomeip dispatch thread.  Qt signal/slot
// connections and QML property bindings must be driven from the Qt main thread.
// QMetaObject::invokeMethod with Qt::QueuedConnection marshals each callback
// safely: the lambda is posted to the Qt event loop and executed on the main
// thread without blocking the vsomeip thread.

void QmlHmiBridge::OnLampStatusUpdated(const LampStatus& lamp_status)
{
    // Capture a value snapshot so the lambda owns its data independently of
    // the caller's lamp_status reference lifetime.
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

// Physical output state (kOn/kOff) — drives button glow, badge, and blink animation.
// Reflects whether the lamp is actually lit, not just whether a command was sent.
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

// command_applied flag — true when the rear node accepted and applied the last
// command.  Distinct from isLampOutputOn: a command may be applied (arbitrated
// through) yet the output_state may still differ briefly before the status event.
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
