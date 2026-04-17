#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

#include "body_control/lighting/application/central_zone_controller.hpp"
#include "body_control/lighting/platform/linux/process_signal_handler.hpp"
#include "body_control/lighting/service/operator_service_provider.hpp"
#include "body_control/lighting/service/rear_lighting_service_consumer.hpp"
#include "body_control/lighting/transport/transport_adapter_interface.hpp"

namespace body_control
{
namespace lighting
{
namespace transport
{
namespace vsomeip
{

std::unique_ptr<TransportAdapterInterface>
CreateCentralZoneControllerVsomeipClientAdapter();

std::unique_ptr<TransportAdapterInterface>
CreateControllerOperatorVsomeipServerAdapter();

}  // namespace vsomeip
}  // namespace transport
}  // namespace lighting
}  // namespace body_control

int main()
{
    using body_control::lighting::application::CentralZoneController;
    using body_control::lighting::application::ControllerStatus;
    using body_control::lighting::platform::linux::ProcessSignalHandler;
    using body_control::lighting::platform::linux::SignalHandlerStatus;
    using body_control::lighting::service::OperatorServiceProvider;
    using body_control::lighting::service::OperatorServiceStatus;
    using body_control::lighting::service::RearLightingServiceConsumer;
    using body_control::lighting::transport::TransportAdapterInterface;

    ProcessSignalHandler signal_handler {};
    if (signal_handler.Install() != SignalHandlerStatus::kSuccess)
    {
        std::cerr << "Failed to install signal handler.\n";
        return 1;
    }

    std::unique_ptr<TransportAdapterInterface> rear_transport =
        body_control::lighting::transport::vsomeip::
            CreateCentralZoneControllerVsomeipClientAdapter();

    if (rear_transport == nullptr)
    {
        std::cerr << "Failed to create rear lighting transport adapter.\n";
        return 1;
    }

    std::unique_ptr<TransportAdapterInterface> operator_transport =
        body_control::lighting::transport::vsomeip::
            CreateControllerOperatorVsomeipServerAdapter();

    if (operator_transport == nullptr)
    {
        std::cerr << "Failed to create operator transport adapter.\n";
        return 1;
    }

    RearLightingServiceConsumer rear_lighting_service_consumer {
        *rear_transport};

    CentralZoneController central_zone_controller {
        rear_lighting_service_consumer};

    OperatorServiceProvider operator_service_provider {
        central_zone_controller,
        *operator_transport};

    const ControllerStatus init_status =
        central_zone_controller.Initialize();

    if (init_status != ControllerStatus::kSuccess)
    {
        std::cerr << "Failed to initialize central zone controller.\n";
        return 1;
    }

    const OperatorServiceStatus operator_init_status =
        operator_service_provider.Initialize();

    if (operator_init_status != OperatorServiceStatus::kSuccess)
    {
        std::cerr << "Failed to initialize operator service provider.\n";
        static_cast<void>(central_zone_controller.Shutdown());
        return 1;
    }

    static_cast<void>(central_zone_controller.RequestNodeHealth());

    std::cout << "Central zone controller is running.\n";
    std::cout << "Press Ctrl+C to shut down.\n";

    while (!ProcessSignalHandler::IsShutdownRequested())
    {
        // Idle — events are driven by the UDP receiver thread.
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    static_cast<void>(operator_service_provider.Shutdown());
    static_cast<void>(central_zone_controller.Shutdown());

    std::cout << "Central zone controller stopped.\n";

    return 0;
}
