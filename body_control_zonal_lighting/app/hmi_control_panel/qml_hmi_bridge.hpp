#pragma once

#include <optional>

#include <QMutex>
#include <QObject>
#include <QTimer>

#include "body_control/lighting/domain/lamp_status_types.hpp"
#include "body_control/lighting/hmi/main_window.hpp"
#include "body_control/lighting/service/operator_service_interface.hpp"

// QObject adapter that bridges C++ service-layer callbacks into QML property bindings.
//
// Threading: On* callbacks (vsomeip thread) ONLY write into pending_ under
// pending_mtx_ and return immediately — zero Qt calls, zero cross-thread posts.
// An 80 ms repeating QTimer (GUI thread) runs pollAndUpdate() at ~12 Hz,
// reads pending_ in one mutex-protected snapshot, forwards to MainWindow, and
// emits Q_PROPERTY change signals only on real value transitions.
// Result: GUI event-queue pressure is O(1) regardless of vsomeip RX burst rate.
class QmlHmiBridge final
    : public QObject
    , public body_control::lighting::service::OperatorServiceEventListenerInterface
{
    Q_OBJECT

    Q_PROPERTY(bool leftOn           READ leftOn           NOTIFY leftOnChanged)
    Q_PROPERTY(bool rightOn          READ rightOn          NOTIFY rightOnChanged)
    Q_PROPERTY(bool hazardOn         READ hazardOn         NOTIFY hazardOnChanged)
    Q_PROPERTY(bool parkOn           READ parkOn           NOTIFY parkOnChanged)
    Q_PROPERTY(bool headOn           READ headOn           NOTIFY headOnChanged)
    Q_PROPERTY(bool leftArrowActive  READ leftArrowActive  NOTIFY displayStateChanged)
    Q_PROPERTY(bool rightArrowActive READ rightArrowActive NOTIFY displayStateChanged)
    Q_PROPERTY(bool ethUp            READ ethUp            NOTIFY ethUpChanged)
    Q_PROPERTY(bool svcUp            READ svcUp            NOTIFY svcUpChanged)
    Q_PROPERTY(bool controllerOnline READ controllerOnline NOTIFY controllerOnlineChanged)
    Q_PROPERTY(bool faultPresent     READ faultPresent     NOTIFY faultStatusChanged)
    Q_PROPERTY(int  activeFaultCount READ activeFaultCount NOTIFY faultStatusChanged)

public:
    explicit QmlHmiBridge(
        body_control::lighting::hmi::MainWindow& main_window,
        QObject* parent = nullptr);

    void OnLampStatusUpdated(
        const body_control::lighting::domain::LampStatus& lamp_status) override;
    void OnNodeHealthUpdated(
        const body_control::lighting::domain::NodeHealthStatus& node_health_status) override;
    void OnControllerAvailabilityChanged(bool is_available) override;

    Q_INVOKABLE void toggleLeftIndicator();
    Q_INVOKABLE void toggleRightIndicator();
    Q_INVOKABLE void toggleHazardLamp();
    Q_INVOKABLE void toggleParkLamp();
    Q_INVOKABLE void toggleHeadLamp();
    Q_INVOKABLE void requestNodeHealth();

    bool leftOn()           const noexcept { return left_on_; }
    bool rightOn()          const noexcept { return right_on_; }
    bool hazardOn()         const noexcept { return hazard_on_; }
    bool parkOn()           const noexcept { return park_on_; }
    bool headOn()           const noexcept { return head_on_; }
    bool leftArrowActive()  const noexcept { return hazard_on_ || left_on_; }
    bool rightArrowActive() const noexcept { return hazard_on_ || right_on_; }
    bool ethUp()            const noexcept { return eth_up_; }
    bool svcUp()            const noexcept { return svc_up_; }
    bool controllerOnline() const noexcept { return controller_online_; }
    bool faultPresent()     const noexcept { return fault_present_; }
    int  activeFaultCount() const noexcept { return static_cast<int>(active_fault_count_); }

signals:
    void leftOnChanged();
    void rightOnChanged();
    void hazardOnChanged();
    void parkOnChanged();
    void headOnChanged();
    void displayStateChanged();
    void ethUpChanged();
    void svcUpChanged();
    void controllerOnlineChanged();
    void faultStatusChanged();

private slots:
    void pollAndUpdate();

private:
    // Pending state: latest value received from the vsomeip thread.
    // Protected by pending_mtx_.  std::nullopt means no update pending.
    // Storing the full status structs here lets flushPending forward them to
    // MainWindow on the GUI thread, eliminating per-event invokeMethod posts.
    struct PendingState {
        // QML property booleans
        std::optional<bool> left_on;
        std::optional<bool> right_on;
        std::optional<bool> hazard_on;
        std::optional<bool> park_on;
        std::optional<bool> head_on;
        std::optional<bool> eth_up;
        std::optional<bool> svc_up;
        std::optional<bool>          controller_online;
        std::optional<bool>          fault_present;
        std::optional<std::uint16_t> active_fault_count;
        // Latest full status for MainWindow terminal forwarding
        std::optional<body_control::lighting::domain::LampStatus> lamp_left;
        std::optional<body_control::lighting::domain::LampStatus> lamp_right;
        std::optional<body_control::lighting::domain::LampStatus> lamp_hazard;
        std::optional<body_control::lighting::domain::LampStatus> lamp_park;
        std::optional<body_control::lighting::domain::LampStatus> lamp_head;
        std::optional<body_control::lighting::domain::NodeHealthStatus> node_health;
    };

    QMutex       pending_mtx_;
    PendingState pending_;
    QTimer*      update_timer_{nullptr};

    body_control::lighting::hmi::MainWindow& main_window_;

    bool left_on_          {false};
    bool right_on_         {false};
    bool hazard_on_        {false};
    bool park_on_          {false};
    bool head_on_          {false};
    bool          eth_up_            {false};
    bool          svc_up_            {false};
    bool          controller_online_ {false};
    bool          fault_present_     {false};
    std::uint16_t active_fault_count_{0U};
};
