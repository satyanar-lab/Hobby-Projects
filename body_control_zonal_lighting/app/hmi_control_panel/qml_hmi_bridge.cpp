#include "qml_hmi_bridge.hpp"

#include <QDateTime>
#include <QDebug>
#include <QThread>

#include "body_control/lighting/hmi/hmi_action_types.hpp"

using body_control::lighting::domain::LampFunction;
using body_control::lighting::domain::LampOutputState;
using body_control::lighting::domain::LampStatus;
using body_control::lighting::domain::NodeHealthStatus;
using body_control::lighting::hmi::HmiAction;
using body_control::lighting::hmi::MainWindow;

// ── Diagnostic helpers ────────────────────────────────────────────────────────
// All LOG-* macros emit to stderr via qDebug so they appear in both the
// terminal and in Qt Creator's Application Output pane.
//
// Interpretation guide (look at which LOG level stops firing first):
//   LOG-0-MAIN-ALIVE  stops → Qt main thread / event loop is hung
//   LOG-1-EVENT-IN    stops → vsomeip IO thread stopped delivering events
//   LOG-2-BRIDGE-SET  stops → kUnknown guard is dropping every event
//   LOG-3-POLL-SNAP   stops → pending_ never has data (vsomeip→Qt handoff broken)
//   LOG-4-EMIT-DONE   stops → value-equality guard suppresses every signal
//   LOG-5 (QML)       stops → QML binding not triggered by the C++ signal

static const char* LampName(const LampFunction fn) noexcept
{
    switch (fn)
    {
    case LampFunction::kLeftIndicator:  return "LEFT";
    case LampFunction::kRightIndicator: return "RIGHT";
    case LampFunction::kHazardLamp:     return "HAZARD";
    case LampFunction::kParkLamp:       return "PARK";
    case LampFunction::kHeadLamp:       return "HEAD";
    default:                            return "UNKNOWN";
    }
}

QmlHmiBridge::QmlHmiBridge(MainWindow& main_window, QObject* parent)
    : QObject(parent)
    , main_window_(main_window)
{
    // 80 ms push-flush timer — runs forever on the GUI thread at ~12 Hz.
    update_timer_ = new QTimer(this);
    update_timer_->setSingleShot(false);
    update_timer_->setInterval(80);
    connect(update_timer_, &QTimer::timeout, this, &QmlHmiBridge::pollAndUpdate);
    update_timer_->start();

    // 2 s pull-backstop timer.
    pull_timer_ = new QTimer(this);
    pull_timer_->setSingleShot(false);
    pull_timer_->setInterval(2000);
    connect(pull_timer_, &QTimer::timeout, this, &QmlHmiBridge::onPullTimerExpired);
    pull_timer_->start();

    // LOG-0: 1 s alive beacon — proves the Qt event loop and GUI thread are live.
    // If this stops appearing in the log while the HMI is frozen, the main
    // thread is hung (blocking call, deadlock, or event-loop exit).
    alive_timer_ = new QTimer(this);
    alive_timer_->setSingleShot(false);
    alive_timer_->setInterval(1000);
    connect(alive_timer_, &QTimer::timeout, this, &QmlHmiBridge::onAliveTimerExpired);
    alive_timer_->start();
}

// LOG-0 slot — GUI thread, fires every 1 s.
void QmlHmiBridge::onAliveTimerExpired()
{
    qDebug() << "[LOG-0-MAIN-ALIVE]" << QDateTime::currentMSecsSinceEpoch()
             << "tid=" << reinterpret_cast<quintptr>(QThread::currentThreadId());
}

// ── OperatorServiceEventListenerInterface callbacks (vsomeip IO thread) ───────

// vsomeip thread — LOG-1 and LOG-2 both fire here.
void QmlHmiBridge::OnLampStatusUpdated(const LampStatus& lamp_status)
{
    const LampStatus snapshot = lamp_status;
    const int fn    = static_cast<int>(snapshot.function);
    const int state = static_cast<int>(snapshot.output_state);

    // LOG-1: vsomeip delivered this event. Fires on every incoming event,
    // including kUnknown-state ones that are about to be dropped.
    qDebug() << "[LOG-1-EVENT-IN]" << QDateTime::currentMSecsSinceEpoch()
             << "lamp=" << fn << "(" << LampName(snapshot.function) << ")"
             << "state=" << state;

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

    // LOG-2: value was accepted and written into pending_.
    // If LOG-1 fires but LOG-2 doesn't, the kUnknown guard dropped the event.
    qDebug() << "[LOG-2-BRIDGE-SET]" << QDateTime::currentMSecsSinceEpoch()
             << "lamp=" << fn << "(" << LampName(snapshot.function) << ")"
             << "pending=" << target;
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

// ── GUI thread — 80 ms flush ──────────────────────────────────────────────────

void QmlHmiBridge::pollAndUpdate()
{
    ++poll_tick_;

    PendingState snap;
    {
        QMutexLocker lock(&pending_mtx_);
        snap     = pending_;
        pending_ = PendingState{};
    }

    // LOG-3: GUI thread is alive and read lamp data from pending_.
    // Logged only when at least one lamp value is present (avoids idle spam).
    // If LOG-2 fires but LOG-3 never shows that lamp, the cross-thread handoff
    // via pending_ is broken (mutex contention, memory visibility issue, etc.).
    const bool has_lamp_data = snap.left_on.has_value()  || snap.right_on.has_value()
                             || snap.hazard_on.has_value() || snap.park_on.has_value()
                             || snap.head_on.has_value();
    if (has_lamp_data)
    {
        qDebug() << "[LOG-3-POLL-SNAP]" << QDateTime::currentMSecsSinceEpoch()
                 << "tick=" << poll_tick_
                 << "left="  << (snap.left_on.has_value()  ? (*snap.left_on  ? 1 : 0) : -1)
                 << "right=" << (snap.right_on.has_value() ? (*snap.right_on ? 1 : 0) : -1)
                 << "haz="   << (snap.hazard_on.has_value()? (*snap.hazard_on? 1 : 0) : -1)
                 << "park="  << (snap.park_on.has_value()  ? (*snap.park_on  ? 1 : 0) : -1)
                 << "head="  << (snap.head_on.has_value()  ? (*snap.head_on  ? 1 : 0) : -1);
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
    // LOG-4 fires immediately after each emit — if LOG-3 shows a new value
    // but LOG-4 doesn't appear, the value-equality guard is suppressing the
    // signal (left_on_ already matches *snap.left_on; no change to report).
    bool display_changed = false;
    if (snap.left_on.has_value() && left_on_ != *snap.left_on) {
        left_on_ = *snap.left_on;
        emit leftOnChanged();
        qDebug() << "[LOG-4-EMIT-DONE]" << QDateTime::currentMSecsSinceEpoch()
                 << "leftOnChanged" << left_on_;
        display_changed = true;
    }
    if (snap.right_on.has_value() && right_on_ != *snap.right_on) {
        right_on_ = *snap.right_on;
        emit rightOnChanged();
        qDebug() << "[LOG-4-EMIT-DONE]" << QDateTime::currentMSecsSinceEpoch()
                 << "rightOnChanged" << right_on_;
        display_changed = true;
    }
    if (snap.hazard_on.has_value() && hazard_on_ != *snap.hazard_on) {
        hazard_on_ = *snap.hazard_on;
        emit hazardOnChanged();
        qDebug() << "[LOG-4-EMIT-DONE]" << QDateTime::currentMSecsSinceEpoch()
                 << "hazardOnChanged" << hazard_on_;
        display_changed = true;
    }
    if (display_changed) { emit displayStateChanged(); }

    if (snap.park_on.has_value() && park_on_ != *snap.park_on) {
        park_on_ = *snap.park_on;
        emit parkOnChanged();
        qDebug() << "[LOG-4-EMIT-DONE]" << QDateTime::currentMSecsSinceEpoch()
                 << "parkOnChanged" << park_on_;
    }
    if (snap.head_on.has_value() && head_on_ != *snap.head_on) {
        head_on_ = *snap.head_on;
        emit headOnChanged();
        qDebug() << "[LOG-4-EMIT-DONE]" << QDateTime::currentMSecsSinceEpoch()
                 << "headOnChanged" << head_on_;
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

// GUI thread — fires every 2 s.
void QmlHmiBridge::onPullTimerExpired()
{
    static_cast<void>(
        main_window_.ProcessAction(HmiAction::kRequestGetAllLampStates));
}

// ── Q_INVOKABLE — GUI thread, non-blocking UDP sends ─────────────────────────

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
