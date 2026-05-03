#include "qml_hmi_bridge.hpp"

#include "body_control/lighting/hmi/hmi_action_types.hpp"

using body_control::lighting::domain::LampFunction;
using body_control::lighting::domain::LampOutputState;
using body_control::lighting::domain::LampStatus;
using body_control::lighting::domain::NodeHealthStatus;
using body_control::lighting::hmi::HmiAction;
using body_control::lighting::hmi::MainWindow;

QmlHmiBridge::QmlHmiBridge(MainWindow& main_window, QObject* parent)
    : QObject(parent)
    , main_window_(main_window)
{
    // Persistent repeating timer — starts once, runs forever at ~12 Hz on the
    // GUI thread.  vsomeip callbacks never post to the GUI event queue at all.
    update_timer_ = new QTimer(this);
    update_timer_->setSingleShot(false);
    update_timer_->setInterval(80);
    connect(update_timer_, &QTimer::timeout, this, &QmlHmiBridge::pollAndUpdate);
    update_timer_->start();
}

// vsomeip thread — write pending_ under mutex and return. No Qt calls.
void QmlHmiBridge::OnLampStatusUpdated(const LampStatus& lamp_status)
{
    const LampStatus snapshot = lamp_status;
    if (snapshot.output_state == LampOutputState::kUnknown) { return; }
    const bool target = (snapshot.output_state == LampOutputState::kOn);

    QMutexLocker lock(&pending_mtx_);
    switch (snapshot.function)
    {
    case LampFunction::kLeftIndicator:
        pending_.left_on   = target;
        pending_.lamp_left = snapshot;
        break;
    case LampFunction::kRightIndicator:
        pending_.right_on   = target;
        pending_.lamp_right = snapshot;
        break;
    case LampFunction::kHazardLamp:
        pending_.hazard_on   = target;
        pending_.lamp_hazard = snapshot;
        break;
    case LampFunction::kParkLamp:
        pending_.park_on   = target;
        pending_.lamp_park = snapshot;
        break;
    case LampFunction::kHeadLamp:
        pending_.head_on   = target;
        pending_.lamp_head = snapshot;
        break;
    default:
        break;
    }
}

// vsomeip thread — write pending_ under mutex and return. No Qt calls.
void QmlHmiBridge::OnNodeHealthUpdated(const NodeHealthStatus& node_health_status)
{
    const NodeHealthStatus snapshot = node_health_status;
    QMutexLocker lock(&pending_mtx_);
    pending_.eth_up             = snapshot.ethernet_link_available;
    pending_.svc_up             = snapshot.service_available;
    pending_.fault_present      = snapshot.lamp_driver_fault_present;
    pending_.active_fault_count = snapshot.active_fault_count;
    pending_.node_health        = snapshot;
}

// vsomeip thread — write pending_ under mutex and return. No Qt calls.
void QmlHmiBridge::OnControllerAvailabilityChanged(bool is_available)
{
    QMutexLocker lock(&pending_mtx_);
    pending_.controller_online = is_available;
}

// GUI thread — runs at ~12 Hz. Snapshot-and-clear under mutex, then apply.
// Idle ticks cost one mutex acquire + has_value() checks and nothing else.
void QmlHmiBridge::pollAndUpdate()
{
    PendingState snap;
    {
        QMutexLocker lock(&pending_mtx_);
        snap     = pending_;
        pending_ = PendingState{};
    }

    // Forward latest status to terminal HMI — safe on GUI thread.
    if (snap.lamp_left.has_value())         { main_window_.OnLampStatusUpdated(*snap.lamp_left); }
    if (snap.lamp_right.has_value())        { main_window_.OnLampStatusUpdated(*snap.lamp_right); }
    if (snap.lamp_hazard.has_value())       { main_window_.OnLampStatusUpdated(*snap.lamp_hazard); }
    if (snap.lamp_park.has_value())         { main_window_.OnLampStatusUpdated(*snap.lamp_park); }
    if (snap.lamp_head.has_value())         { main_window_.OnLampStatusUpdated(*snap.lamp_head); }
    if (snap.node_health.has_value())       { main_window_.OnNodeHealthUpdated(*snap.node_health); }
    if (snap.controller_online.has_value()) {
        main_window_.OnControllerAvailabilityChanged(*snap.controller_online);
    }

    // Emit Q_PROPERTY change signals only on real value transitions.
    bool display_changed = false;
    if (snap.left_on.has_value() && left_on_ != *snap.left_on) {
        left_on_ = *snap.left_on;
        emit leftOnChanged();
        display_changed = true;
    }
    if (snap.right_on.has_value() && right_on_ != *snap.right_on) {
        right_on_ = *snap.right_on;
        emit rightOnChanged();
        display_changed = true;
    }
    if (snap.hazard_on.has_value() && hazard_on_ != *snap.hazard_on) {
        hazard_on_ = *snap.hazard_on;
        emit hazardOnChanged();
        display_changed = true;
    }
    if (display_changed) { emit displayStateChanged(); }

    if (snap.park_on.has_value() && park_on_ != *snap.park_on) {
        park_on_ = *snap.park_on;
        emit parkOnChanged();
    }
    if (snap.head_on.has_value() && head_on_ != *snap.head_on) {
        head_on_ = *snap.head_on;
        emit headOnChanged();
    }
    if (snap.eth_up.has_value() && eth_up_ != *snap.eth_up) {
        eth_up_ = *snap.eth_up;
        emit ethUpChanged();
    }
    if (snap.svc_up.has_value() && svc_up_ != *snap.svc_up) {
        svc_up_ = *snap.svc_up;
        emit svcUpChanged();
    }
    if (snap.controller_online.has_value() && controller_online_ != *snap.controller_online) {
        controller_online_ = *snap.controller_online;
        emit controllerOnlineChanged();
    }
    bool fault_changed = false;
    if (snap.fault_present.has_value() && fault_present_ != *snap.fault_present) {
        fault_present_ = *snap.fault_present;
        fault_changed  = true;
    }
    if (snap.active_fault_count.has_value() && active_fault_count_ != *snap.active_fault_count) {
        active_fault_count_ = *snap.active_fault_count;
        fault_changed       = true;
    }
    if (fault_changed) { emit faultStatusChanged(); }
}

// Q_INVOKABLE methods — ProcessAction is a non-blocking UDP send (microseconds),
// safe to call directly on the GUI thread.
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
