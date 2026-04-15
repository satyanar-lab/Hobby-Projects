#include <atomic>
#include <chrono>
#include <csignal>
#include <thread>

#include "body_control/lighting/application/central_zone_controller.hpp"
#include "body_control/lighting/platform/linux/linux_diagnostic_logger.hpp"
#include "body_control/lighting/service/rear_lighting_service_consumer.hpp"
#include "body_control/lighting/transport/vsomeip/vsomeip_client_adapter.hpp"
#include "body_control/lighting/transport/vsomeip/vsomeip_runtime_manager.hpp"

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
    diagnostic_logger.LogInfo("Central zone controller starting");

    transport::vsomeip::VsomeipRuntimeManager runtime_manager {};
    transport::vsomeip::VsomeipClientAdapter transport_adapter {runtime_manager};
    service::RearLightingServiceConsumer rear_lighting_service {transport_adapter};
    application::CentralZoneController central_zone_controller {
        rear_lighting_service};

    rear_lighting_service.SetEventListener(&central_zone_controller);

    const service::ServiceStatus initialize_status =
        rear_lighting_service.Initialize();

    if (initialize_status != service::ServiceStatus::kSuccess)
    {
        diagnostic_logger.LogError(
            "Failed to initialize rear lighting service consumer");
        return 1;
    }

    if (!central_zone_controller.Initialize())
    {
        diagnostic_logger.LogError("Failed to initialize central zone controller");
        static_cast<void>(rear_lighting_service.Shutdown());
        return 1;
    }

    diagnostic_logger.LogInfo("Central zone controller is running");

    while (g_keep_running.load())
    {
        if (rear_lighting_service.IsServiceAvailable())
        {
            static_cast<void>(rear_lighting_service.RequestNodeHealth());
        }

        std::this_thread::sleep_for(std::chrono::milliseconds {500});
    }

    diagnostic_logger.LogInfo("Central zone controller shutting down");

    static_cast<void>(rear_lighting_service.Shutdown());

    return 0;
}