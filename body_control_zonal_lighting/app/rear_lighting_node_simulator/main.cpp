#include <iostream>

#include "body_control/lighting/application/rear_lighting_function_manager.hpp"
#include "body_control/lighting/service/rear_lighting_service_provider.hpp"
#include "body_control/lighting/transport/transport_adapter_interface.hpp"

namespace body_control
{
namespace lighting
{
namespace
{

class NodeTransportStub final : public transport::TransportAdapterInterface
{
public:
    NodeTransportStub() noexcept
        : message_handler_(nullptr)
    {
    }

    transport::TransportStatus Initialize() override
    {
        if (message_handler_ != nullptr)
        {
            message_handler_->OnTransportAvailabilityChanged(true);
        }

        return transport::TransportStatus::kSuccess;
    }

    transport::TransportStatus Shutdown() override
    {
        if (message_handler_ != nullptr)
        {
            message_handler_->OnTransportAvailabilityChanged(false);
        }

        return transport::TransportStatus::kSuccess;
    }

    transport::TransportStatus SendRequest(
        const transport::TransportMessage& transport_message) override
    {
        static_cast<void>(transport_message);
        return transport::TransportStatus::kSuccess;
    }

    transport::TransportStatus SendResponse(
        const transport::TransportMessage& transport_message) override
    {
        static_cast<void>(transport_message);
        return transport::TransportStatus::kSuccess;
    }

    transport::TransportStatus SendEvent(
        const transport::TransportMessage& transport_message) override
    {
        static_cast<void>(transport_message);
        return transport::TransportStatus::kSuccess;
    }

    void SetMessageHandler(
        transport::TransportMessageHandlerInterface* const message_handler) noexcept override
    {
        message_handler_ = message_handler;
    }

private:
    transport::TransportMessageHandlerInterface* message_handler_;
};

}  // namespace
}  // namespace lighting
}  // namespace body_control

int main()
{
    body_control::lighting::application::RearLightingFunctionManager
        rear_lighting_function_manager {};
    body_control::lighting::NodeTransportStub transport_adapter {};
    body_control::lighting::service::RearLightingServiceProvider
        rear_lighting_service_provider {
            rear_lighting_function_manager,
            transport_adapter};

    const body_control::lighting::service::ServiceStatus init_status =
        rear_lighting_service_provider.Initialize();

    if (init_status != body_control::lighting::service::ServiceStatus::kSuccess)
    {
        std::cerr << "Failed to initialize rear lighting node simulator.\n";
        return 1;
    }

    std::cout << "Rear lighting node simulator initialized.\n";
    std::cout << "Provider is ready for command handling and status publication.\n";

    static_cast<void>(rear_lighting_service_provider.Shutdown());
    return 0;
}