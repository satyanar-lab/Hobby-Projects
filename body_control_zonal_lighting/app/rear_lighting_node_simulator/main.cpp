#include <iostream>
#include <memory>

#include "body_control/lighting/application/rear_lighting_function_manager.hpp"
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
    using body_control::lighting::service::RearLightingServiceProvider;
    using body_control::lighting::service::ServiceStatus;
    using body_control::lighting::transport::TransportAdapterInterface;

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
    std::cout << "Press ENTER to shut down.\n";

    std::cin.get();

    const ServiceStatus shutdown_status =
        rear_lighting_service_provider.Shutdown();

    if (shutdown_status != ServiceStatus::kSuccess)
    {
        std::cerr << "Rear lighting node simulator shutdown completed with errors.\n";
        return 1;
    }

    return 0;
}