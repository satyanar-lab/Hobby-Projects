#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

#include "body_control/lighting/application/rear_lighting_function_manager.hpp"
#include "body_control/lighting/domain/lighting_constants.hpp"
#include "body_control/lighting/platform/linux/process_signal_handler.hpp"
#include "body_control/lighting/service/rear_lighting_service_provider.hpp"
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
CreateRearLightingNodeVsomeipServerAdapter();

}  // namespace vsomeip
}  // namespace transport
}  // namespace lighting
}  // namespace body_control

int main()
{
    using body_control::lighting::application::RearLightingFunctionManager;
    using body_control::lighting::domain::timing::kLampStatusPublishPeriod;
    using body_control::lighting::domain::timing::kMainLoopPeriod;
    using body_control::lighting::domain::timing::kNodeHealthPublishPeriod;
    using body_control::lighting::platform::linux::ProcessSignalHandler;
    using body_control::lighting::platform::linux::SignalHandlerStatus;
    using body_control::lighting::service::RearLightingServiceProvider;
    using body_control::lighting::service::ServiceStatus;
    using body_control::lighting::transport::TransportAdapterInterface;

    ProcessSignalHandler signal_handler {};

    if (signal_handler.Install() != SignalHandlerStatus::kSuccess)
    {
        std::cerr << "Failed to install signal handler.\n";
        return 1;
    }

    RearLightingFunctionManager rear_lighting_function_manager {};

    std::unique_ptr<TransportAdapterInterface> transport_adapter =
        body_control::lighting::transport::vsomeip::
            CreateRearLightingNodeVsomeipServerAdapter();

    if (transport_adapter == nullptr)
    {
        std::cerr << "Failed to create rear lighting node transport adapter.\n";
        return 1;
    }

    RearLightingServiceProvider rear_lighting_service_provider {
        rear_lighting_function_manager,
        *transport_adapter};

    const ServiceStatus init_status =
        rear_lighting_service_provider.Initialize();

    if (init_status != ServiceStatus::kSuccess)
    {
        std::cerr << "Failed to initialize rear lighting node simulator.\n";
        return 1;
    }

    std::cout << "Rear lighting node simulator is running.\n";
    std::cout << "Send SIGINT or SIGTERM to shut down.\n";

    std::chrono::milliseconds lamp_elapsed {0};
    std::chrono::milliseconds health_elapsed {0};

    while (!ProcessSignalHandler::IsShutdownRequested())
    {
        std::this_thread::sleep_for(kMainLoopPeriod);
        lamp_elapsed += kMainLoopPeriod;
        health_elapsed += kMainLoopPeriod;

        if (lamp_elapsed >= kLampStatusPublishPeriod)
        {
            rear_lighting_service_provider.BroadcastAllLampStatuses();
            lamp_elapsed = std::chrono::milliseconds {0};
        }

        if (health_elapsed >= kNodeHealthPublishPeriod)
        {
            rear_lighting_service_provider.BroadcastNodeHealth();
            health_elapsed = std::chrono::milliseconds {0};
        }
    }

    const ServiceStatus shutdown_status =
        rear_lighting_service_provider.Shutdown();

    if (shutdown_status != ServiceStatus::kSuccess)
    {
        std::cerr << "Rear lighting node simulator shutdown completed with errors.\n";
        return 1;
    }

    return 0;
}
