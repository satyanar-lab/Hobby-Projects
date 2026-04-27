#pragma once

#include <QObject>

#include "body_control/lighting/domain/lamp_status_types.hpp"
#include "body_control/lighting/hmi/main_window.hpp"
#include "body_control/lighting/service/operator_service_interface.hpp"

// QObject adapter that bridges the C++ service-layer callbacks into Qt signals.
// Lives entirely in app/hmi_control_panel/ — Qt never leaks into the domain or
// service layers.
//
// Threading: OnLamp*/OnNodeHealth* arrive on the vsomeip/UDP worker thread.
// All Qt object mutations are marshalled to the GUI thread via
// QMetaObject::invokeMethod with Qt::QueuedConnection.
class QmlHmiBridge final
    : public QObject
    , public body_control::lighting::service::OperatorServiceEventListenerInterface
{
    Q_OBJECT

    Q_PROPERTY(bool controllerAvailable
               READ controllerAvailable
               NOTIFY controllerAvailableChanged)
    Q_PROPERTY(bool ethUp       READ ethUp       NOTIFY nodeHealthChanged)
    Q_PROPERTY(bool svcUp       READ svcUp       NOTIFY nodeHealthChanged)
    Q_PROPERTY(bool faultPresent READ faultPresent NOTIFY nodeHealthChanged)
    Q_PROPERTY(int  faultCount  READ faultCount  NOTIFY nodeHealthChanged)
    Q_PROPERTY(int  healthState READ healthState NOTIFY nodeHealthChanged)

public:
    explicit QmlHmiBridge(
        body_control::lighting::hmi::MainWindow& main_window,
        QObject* parent = nullptr);

    // OperatorServiceEventListenerInterface — called on worker thread.
    void OnLampStatusUpdated(
        const body_control::lighting::domain::LampStatus& lamp_status) override;
    void OnNodeHealthUpdated(
        const body_control::lighting::domain::NodeHealthStatus& node_health_status)
        override;
    void OnControllerAvailabilityChanged(bool is_available) override;

    // Called from QML to send lamp toggle commands.
    Q_INVOKABLE void toggleLeftIndicator();
    Q_INVOKABLE void toggleRightIndicator();
    Q_INVOKABLE void toggleHazardLamp();
    Q_INVOKABLE void toggleParkLamp();
    Q_INVOKABLE void toggleHeadLamp();
    Q_INVOKABLE void requestNodeHealth();

    // Called from QML to query current lamp state after lampStatusChanged fires.
    Q_INVOKABLE bool isLampOutputOn(int lamp_function) const noexcept;
    Q_INVOKABLE bool isLampCommandActive(int lamp_function) const noexcept;

    // Q_PROPERTY accessors.
    bool controllerAvailable() const noexcept { return controller_available_; }
    bool ethUp()        const noexcept { return eth_up_; }
    bool svcUp()        const noexcept { return svc_up_; }
    bool faultPresent() const noexcept { return fault_present_; }
    int  faultCount()   const noexcept { return fault_count_; }
    int  healthState()  const noexcept { return health_state_; }

signals:
    void lampStatusChanged(int lampFunction);
    void nodeHealthChanged();
    void controllerAvailableChanged(bool available);

private:
    body_control::lighting::hmi::MainWindow& main_window_;
    bool controller_available_ {false};
    bool eth_up_               {false};
    bool svc_up_               {false};
    bool fault_present_        {false};
    int  fault_count_          {0};
    int  health_state_         {0};
};
