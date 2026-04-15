#include <iostream>
#include <memory>

#include "body_control/lighting/application/central_zone_controller.hpp"
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

}  // namespace vsomeip
}  // namespace transport
}  // namespace lighting
}  // namespace body_control

int main()
{
    using body_control::lighting::application::CentralZoneController;
    using body_control::lighting::application::ControllerStatus;
    using body_control::lighting::service::RearLightingServiceConsumer;
    using body_control::lighting::transport::TransportAdapterInterface;

    std::unique_ptr<TransportAdapterInterface> transport_adapter =
        body_control::lighting::transport::vsomeip::
            CreateCentralZoneControllerVsomeipClientAdapter();

    if (transport_adapter == nullptr)
    {
        std::cerr << "Failed to create central zone controller transport adapter.\n";
        return 1;
    }

    RearLightingServiceConsumer rear_lighting_service_consumer {
        *transport_adapter};

    CentralZoneController central_zone_controller {
        rear_lighting_service_consumer};

    const ControllerStatus init_status =
        central_zone_controller.Initialize();

    if (init_status != ControllerStatus::kSuccess)
    {
        std::cerr << "Failed to initialize central zone controller.\n";
        return 1;
    }

    static_cast<void>(central_zone_controller.RequestNodeHealth());

    std::cout << "Central zone controller is running.\n";
    std::cout << "Press ENTER to shut down.\n";

    std::cin.get();

    const ControllerStatus shutdown_status =
        central_zone_controller.Shutdown();

    if (shutdown_status != ControllerStatus::kSuccess)
    {
        std::cerr << "Central zone controller shutdown completed with errors.\n";
        return 1;
    }

    return 0;
}