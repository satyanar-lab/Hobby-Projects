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

namespace ethernet
{

std::unique_ptr<TransportAdapterInterface>
CreateDirectUdpTransportAdapter(
    const char*   remote_ip,
    std::uint16_t remote_port);

}  // namespace ethernet
}  // namespace transport
}  // namespace lighting
}  // namespace body_control

namespace
{

// Forwards every send to both primary (vsomeip) and secondary (NUCLEO UDP).
// Secondary failures are silently ignored — it is a best-effort path.
//
// Also acts as a proxy TransportMessageHandlerInterface so it can intercept
// OnTransportAvailabilityChanged from vsomeip: the fanout always reports
// available=true to the consumer once initialized, regardless of whether
// vsomeip has completed service discovery.  Without this, the consumer's
// is_service_available_ guard blocks all sends when the simulator is not
// running, so the NUCLEO never receives commands.
class FanoutTransportAdapter final
    : public body_control::lighting::transport::TransportAdapterInterface
    , public body_control::lighting::transport::TransportMessageHandlerInterface
{
    using TAI  = body_control::lighting::transport::TransportAdapterInterface;
    using TMHI = body_control::lighting::transport::TransportMessageHandlerInterface;
    using TM   = body_control::lighting::transport::TransportMessage;
    using TS   = body_control::lighting::transport::TransportStatus;

public:
    FanoutTransportAdapter(
        std::unique_ptr<TAI> primary,
        std::unique_ptr<TAI> secondary) noexcept
        : primary_(std::move(primary))
        , secondary_(std::move(secondary))
        , outer_handler_(nullptr)
    {
    }

    ~FanoutTransportAdapter() override
    {
        static_cast<void>(Shutdown());
    }

    [[nodiscard]] TS Initialize() override
    {
        static_cast<void>(secondary_->Initialize());
        const TS result = primary_->Initialize();
        // Report available immediately — do not wait for vsomeip discovery.
        if (outer_handler_ != nullptr)
        {
            outer_handler_->OnTransportAvailabilityChanged(true);
        }
        return result;
    }

    [[nodiscard]] TS Shutdown() override
    {
        static_cast<void>(secondary_->Shutdown());
        return primary_->Shutdown();
    }

    [[nodiscard]] TS SendRequest(const TM& msg) override
    {
        static_cast<void>(secondary_->SendRequest(msg));
        return primary_->SendRequest(msg);
    }

    [[nodiscard]] TS SendResponse(const TM& msg) override
    {
        static_cast<void>(secondary_->SendResponse(msg));
        return primary_->SendResponse(msg);
    }

    [[nodiscard]] TS SendEvent(const TM& msg) override
    {
        static_cast<void>(secondary_->SendEvent(msg));
        return primary_->SendEvent(msg);
    }

    // Route primary's handler through this proxy so availability is intercepted.
    void SetMessageHandler(TMHI* handler) noexcept override
    {
        outer_handler_ = handler;
        primary_->SetMessageHandler(this);
    }

    // Proxy: suppress availability changes from vsomeip — always stay available.
    void OnTransportAvailabilityChanged(bool /*is_available*/) override
    {
        if (outer_handler_ != nullptr)
        {
            outer_handler_->OnTransportAvailabilityChanged(true);
        }
    }

    // Proxy: pass received messages straight through to the consumer.
    void OnTransportMessageReceived(const TM& msg) override
    {
        if (outer_handler_ != nullptr)
        {
            outer_handler_->OnTransportMessageReceived(msg);
        }
    }

private:
    std::unique_ptr<TAI> primary_;
    std::unique_ptr<TAI> secondary_;
    TMHI*                outer_handler_;
};

}  // namespace

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

    std::unique_ptr<TransportAdapterInterface> vsomeip_rear_transport =
        body_control::lighting::transport::vsomeip::
            CreateCentralZoneControllerVsomeipClientAdapter();

    if (vsomeip_rear_transport == nullptr)
    {
        std::cerr << "Failed to create rear lighting vsomeip transport adapter.\n";
        return 1;
    }

    std::unique_ptr<TransportAdapterInterface> nucleo_transport =
        body_control::lighting::transport::ethernet::
            CreateDirectUdpTransportAdapter("192.168.0.20", 41001U);

    std::unique_ptr<TransportAdapterInterface> rear_transport =
        std::make_unique<FanoutTransportAdapter>(
            std::move(vsomeip_rear_transport),
            std::move(nucleo_transport));

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
