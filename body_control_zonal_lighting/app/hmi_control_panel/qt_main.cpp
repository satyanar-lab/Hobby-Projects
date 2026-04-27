#include <iostream>
#include <memory>

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include "body_control/lighting/domain/lighting_service_ids.hpp"
#include "body_control/lighting/hmi/main_window.hpp"
#include "body_control/lighting/service/operator_service_consumer.hpp"
#include "body_control/lighting/transport/transport_adapter_interface.hpp"
#include "qml_hmi_bridge.hpp"

namespace body_control
{
namespace lighting
{
namespace transport
{
namespace vsomeip
{

std::unique_ptr<body_control::lighting::transport::TransportAdapterInterface>
CreateOperatorClientVsomeipClientAdapter();

}  // namespace vsomeip
}  // namespace transport
}  // namespace lighting
}  // namespace body_control

int main(int argc, char* argv[])
{
    using body_control::lighting::domain::operator_service::kHmiControlPanelApplicationId;
    using body_control::lighting::hmi::MainWindow;
    using body_control::lighting::service::OperatorServiceConsumer;
    using body_control::lighting::service::OperatorServiceStatus;
    using body_control::lighting::transport::TransportAdapterInterface;

    QGuiApplication app(argc, argv);
    app.setApplicationName("Body Control Lighting HMI");
    app.setOrganizationName("satyanar-lab");

    std::unique_ptr<TransportAdapterInterface> transport_adapter =
        body_control::lighting::transport::vsomeip::
            CreateOperatorClientVsomeipClientAdapter();

    if (transport_adapter == nullptr)
    {
        std::cerr << "Failed to create HMI transport adapter.\n";
        return 1;
    }

    OperatorServiceConsumer operator_service {
        *transport_adapter, kHmiControlPanelApplicationId};

    MainWindow main_window {operator_service};

    QmlHmiBridge bridge {main_window};

    operator_service.SetEventListener(&bridge);

    const OperatorServiceStatus init_status = operator_service.Initialize();
    if (init_status != OperatorServiceStatus::kSuccess)
    {
        std::cerr << "Failed to initialize HMI operator service.\n";
        return 1;
    }

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("hmi", &bridge);
    engine.load(QUrl(QStringLiteral("qrc:/LightingHmi.qml")));

    if (engine.rootObjects().isEmpty())
    {
        std::cerr << "Failed to load QML dashboard.\n";
        static_cast<void>(operator_service.Shutdown());
        return 1;
    }

    const int result = QGuiApplication::exec();
    static_cast<void>(operator_service.Shutdown());
    return result;
}
