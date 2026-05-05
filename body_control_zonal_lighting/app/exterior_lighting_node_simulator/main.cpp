#include <chrono>
#include <cstring>
#include <iostream>
#include <memory>
#include <thread>

#include "body_control/lighting/application/exterior_lighting_function_manager.hpp"
#include "body_control/lighting/application/uds_request_handler.hpp"
#include "body_control/lighting/domain/lighting_constants.hpp"
#include "body_control/lighting/domain/lighting_service_ids.hpp"
#include "body_control/lighting/platform/linux/process_signal_handler.hpp"
#include "body_control/lighting/service/exterior_lighting_service_provider.hpp"
#include "body_control/lighting/transport/doip_server.hpp"
#include "body_control/lighting/transport/some_ip_sd_offerer.hpp"
#include "body_control/lighting/transport/some_ip_sd_types.hpp"
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
CreateExteriorLightingNodeVsomeipServerAdapter();

}  // namespace vsomeip
}  // namespace transport
}  // namespace lighting
}  // namespace body_control

int main()
{
    using body_control::lighting::application::ExteriorLightingFunctionManager;
    using body_control::lighting::application::UdsRequestHandler;
    using body_control::lighting::domain::timing::kLampStatusPublishPeriod;
    using body_control::lighting::domain::timing::kMainLoopPeriod;
    using body_control::lighting::domain::timing::kNodeHealthPublishPeriod;
    using body_control::lighting::platform::linux::ProcessSignalHandler;
    using body_control::lighting::platform::linux::SignalHandlerStatus;
    using body_control::lighting::service::ExteriorLightingServiceProvider;
    using body_control::lighting::service::ServiceStatus;
    using body_control::lighting::transport::DoipServer;
    using body_control::lighting::transport::SomeIpSdOfferer;
    using body_control::lighting::transport::ServiceOffer;
    using body_control::lighting::transport::SdStatus;
    using body_control::lighting::transport::TransportAdapterInterface;

    ProcessSignalHandler signal_handler {};

    if (signal_handler.Install() != SignalHandlerStatus::kSuccess)
    {
        std::cerr << "Failed to install signal handler.\n";
        return 1;
    }

    ExteriorLightingFunctionManager exterior_lighting_function_manager {};

    // DoIP diagnostic server — runs on TCP :13400 parallel to the SOME/IP path.
    UdsRequestHandler uds_handler {exterior_lighting_function_manager};
    DoipServer        doip_server  {uds_handler};

    std::unique_ptr<TransportAdapterInterface> transport_adapter =
        body_control::lighting::transport::vsomeip::
            CreateExteriorLightingNodeVsomeipServerAdapter();

    if (transport_adapter == nullptr)
    {
        std::cerr << "Failed to create exterior lighting node transport adapter.\n";
        return 1;
    }

    ExteriorLightingServiceProvider exterior_lighting_service_provider {
        exterior_lighting_function_manager,
        *transport_adapter};

    const ServiceStatus init_status =
        exterior_lighting_service_provider.Initialize();

    if (init_status != ServiceStatus::kSuccess)
    {
        std::cerr << "Failed to initialize exterior lighting node simulator.\n";
        return 1;
    }

    doip_server.Start();

    // SOME/IP-SD: advertise ExteriorLightingService on the local network so the
    // CZC can discover it dynamically instead of relying on a static endpoint.
    SomeIpSdOfferer sd_offerer {};
    ServiceOffer offer {};
    offer.service_id    = body_control::lighting::domain::exterior_lighting_service::kServiceId;
    offer.instance_id   = body_control::lighting::domain::exterior_lighting_service::kInstanceId;
    offer.major_version = 1U;
    offer.ttl_seconds   = 5U;
    offer.local_port    = 41001U;
    // Advertise the loopback address so the collocated CZC finds us without
    // requiring a physical NIC.  Replace with the real host IP when running
    // CZC and simulator on separate machines.
    std::strncpy(offer.local_ip, "127.0.0.1", sizeof(offer.local_ip) - 1U);

    if (sd_offerer.Init(offer) == SdStatus::kSuccess)
    {
        static_cast<void>(sd_offerer.Start());
        std::cout << "[SD] OfferService started for 0x"
                  << std::hex << offer.service_id << std::dec
                  << " on 224.244.224.245:30490 every "
                  << offer.period_ms << " ms\n";
    }
    else
    {
        std::cerr << "[SD] OfferService init failed (multicast unavailable?)\n";
    }

    std::cout << "Exterior lighting node simulator is running.\n";
    std::cout << "Send SIGINT or SIGTERM to shut down.\n";

    // Two independent accumulators so lamp and health can have different
    // publish periods without one blocking the other.
    std::chrono::milliseconds lamp_elapsed {0};
    std::chrono::milliseconds health_elapsed {0};

    while (!ProcessSignalHandler::IsShutdownRequested())
    {
        std::this_thread::sleep_for(kMainLoopPeriod);
        lamp_elapsed   += kMainLoopPeriod;
        health_elapsed += kMainLoopPeriod;

        if (lamp_elapsed >= kLampStatusPublishPeriod)
        {
            // Periodic push so the CZC cache stays current even if no command
            // arrived since the last broadcast.
            exterior_lighting_service_provider.BroadcastAllLampStatuses();
            lamp_elapsed = std::chrono::milliseconds {0};
        }

        if (health_elapsed >= kNodeHealthPublishPeriod)
        {
            exterior_lighting_service_provider.BroadcastNodeHealth();
            health_elapsed = std::chrono::milliseconds {0};
        }
    }

    static_cast<void>(sd_offerer.Stop());
    doip_server.Stop();

    const ServiceStatus shutdown_status =
        exterior_lighting_service_provider.Shutdown();

    if (shutdown_status != ServiceStatus::kSuccess)
    {
        std::cerr << "Exterior lighting node simulator shutdown completed with errors.\n";
        return 1;
    }

    return 0;
}
