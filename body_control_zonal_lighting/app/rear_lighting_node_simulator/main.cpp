#include <atomic>
#include <chrono>
#include <csignal>
#include <thread>

#include "body_control/lighting/application/rear_lighting_function_manager.hpp"
#include "body_control/lighting/domain/lighting_types.hpp"
#include "body_control/lighting/platform/linux/linux_diagnostic_logger.hpp"
#include "body_control/lighting/service/rear_lighting_service_provider.hpp"
#include "body_control/lighting/transport/vsomeip/vsomeip_runtime_manager.hpp"
#include "body_control/lighting/transport/vsomeip/vsomeip_server_adapter.hpp"

namespace
{

std::atomic<bool> g_keep_running {true};

void HandleSignal(int signal_number)
{
    static_cast<void>(signal_number);
    g_keep_running.store(false);
}

}  // namespace

int main()
{
    using namespace body_control::lighting;

    std::signal(SIGINT, HandleSignal);
    std::signal(SIGTERM, HandleSignal);

    platform::linux::LinuxDiagnosticLogger diagnostic_logger {};
    static_cast<void>(diagnostic_logger.Initialize());
    diagnostic_logger.LogInfo("Rear lighting node simulator starting");

    transport::vsomeip::VsomeipRuntimeManager runtime_manager {};
    transport::vsomeip::VsomeipServerAdapter transport_adapter {runtime_manager};
    application::RearLightingFunctionManager rear_lighting_function_manager {};
    service::RearLightingServiceProvider rear_lighting_service_provider {
        transport_adapter,
        rear_lighting_function_manager};

    if (rear_lighting_function_manager.Initialize() !=
        application::FunctionManagerStatus::kSuccess)
    {
        diagnostic_logger.LogError("Failed to initialize rear lighting function manager");
        return 1;
    }

    if (rear_lighting_service_provider.Initialize() !=
        service::ServiceStatus::kSuccess)
    {
        diagnostic_logger.LogError("Failed to initialize rear lighting service provider");
        return 1;
    }

    domain::NodeHealthStatus node_health_status {};
    node_health_status.health_state = domain::NodeHealthState::kHealthy;
    node_health_status.ethernet_link_available = true;
    node_health_status.service_available = true;
    node_health_status.lamp_driver_fault_present = false;
    node_health_status.active_fault_count = 0U;

    rear_lighting_service_provider.UpdateNodeHealthStatus(node_health_status);
    static_cast<void>(
        rear_lighting_service_provider.PublishNodeHealthStatus(node_health_status));

    diagnostic_logger.LogInfo("Rear lighting node simulator running");

    while (g_keep_running.load())
    {
        static_cast<void>(
            rear_lighting_service_provider.PublishNodeHealthStatus(node_health_status));

        std::this_thread::sleep_for(std::chrono::milliseconds {500});
    }

    diagnostic_logger.LogInfo("Rear lighting node simulator shutting down");

    static_cast<void>(rear_lighting_service_provider.Shutdown());

    return 0;
}