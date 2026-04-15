#include <iostream>

#include "body_control/lighting/application/central_zone_controller.hpp"
#include "body_control/lighting/service/rear_lighting_service_consumer.hpp"
#include "body_control/lighting/transport/someip_message_builder.hpp"
#include "body_control/lighting/transport/someip_message_parser.hpp"
#include "body_control/lighting/transport/transport_adapter_interface.hpp"

namespace body_control
{
namespace lighting
{
namespace
{

class LoopbackTransportStub final : public transport::TransportAdapterInterface
{
public:
    LoopbackTransportStub() noexcept
        : message_handler_(nullptr)
    {
        lamp_statuses_[0U].function = domain::LampFunction::kLeftIndicator;
        lamp_statuses_[1U].function = domain::LampFunction::kRightIndicator;
        lamp_statuses_[2U].function = domain::LampFunction::kHazardLamp;
        lamp_statuses_[3U].function = domain::LampFunction::kParkLamp;
        lamp_statuses_[4U].function = domain::LampFunction::kHeadLamp;
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
        if (message_handler_ == nullptr)
        {
            return transport::TransportStatus::kNotInitialized;
        }

        if (transport::SomeipMessageParser::IsSetLampCommandRequest(
                transport_message))
        {
            const domain::LampCommand lamp_command =
                transport::SomeipMessageParser::ParseLampCommand(
                    transport_message);

            const std::size_t index = LampFunctionToIndex(lamp_command.function);

            if (index >= 5U)
            {
                return transport::TransportStatus::kInvalidArgument;
            }

            lamp_statuses_[index].function = lamp_command.function;
            lamp_statuses_[index].output_state =
                (lamp_command.action == domain::LampCommandAction::kActivate)
                    ? domain::LampOutputState::kOn
                    : domain::LampOutputState::kOff;
            lamp_statuses_[index].command_applied = true;
            lamp_statuses_[index].last_sequence_counter =
                lamp_command.sequence_counter;

            const transport::TransportMessage response =
                transport::SomeipMessageBuilder::BuildLampStatusEvent(
                    lamp_statuses_[index]);

            message_handler_->OnTransportMessageReceived(response);
            return transport::TransportStatus::kSuccess;
        }

        if (transport::SomeipMessageParser::IsGetLampStatusRequest(
                transport_message))
        {
            const domain::LampFunction lamp_function =
                transport::SomeipMessageParser::ParseLampFunction(
                    transport_message);

            const std::size_t index = LampFunctionToIndex(lamp_function);

            if (index >= 5U)
            {
                return transport::TransportStatus::kInvalidArgument;
            }

            const transport::TransportMessage response =
                transport::SomeipMessageBuilder::BuildLampStatusEvent(
                    lamp_statuses_[index]);

            message_handler_->OnTransportMessageReceived(response);
            return transport::TransportStatus::kSuccess;
        }

        if (transport::SomeipMessageParser::IsGetNodeHealthRequest(
                transport_message))
        {
            domain::NodeHealthStatus node_health_status {};
            node_health_status.node_state = domain::NodeHealthState::kOperational;
            node_health_status.ethernet_link_up = true;
            node_health_status.service_available = true;
            node_health_status.last_sequence_counter = 0U;

            const transport::TransportMessage response =
                transport::SomeipMessageBuilder::BuildNodeHealthEvent(
                    node_health_status);

            message_handler_->OnTransportMessageReceived(response);
            return transport::TransportStatus::kSuccess;
        }

        return transport::TransportStatus::kInvalidArgument;
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
    static std::size_t LampFunctionToIndex(
        const domain::LampFunction lamp_function) noexcept
    {
        std::size_t index {5U};

        switch (lamp_function)
        {
        case domain::LampFunction::kLeftIndicator:
            index = 0U;
            break;

        case domain::LampFunction::kRightIndicator:
            index = 1U;
            break;

        case domain::LampFunction::kHazardLamp:
            index = 2U;
            break;

        case domain::LampFunction::kParkLamp:
            index = 3U;
            break;

        case domain::LampFunction::kHeadLamp:
            index = 4U;
            break;

        case domain::LampFunction::kUnknown:
        default:
            break;
        }

        return index;
    }

    transport::TransportMessageHandlerInterface* message_handler_;
    domain::LampStatus lamp_statuses_[5U];
};

}  // namespace
}  // namespace lighting
}  // namespace body_control

int main()
{
    body_control::lighting::LoopbackTransportStub transport_adapter {};
    body_control::lighting::service::RearLightingServiceConsumer
        rear_lighting_service_consumer {transport_adapter};
    body_control::lighting::application::CentralZoneController
        central_zone_controller {rear_lighting_service_consumer};

    const body_control::lighting::application::ControllerStatus init_status =
        central_zone_controller.Initialize();

    if (init_status !=
        body_control::lighting::application::ControllerStatus::kSuccess)
    {
        std::cerr << "Failed to initialize central zone controller.\n";
        return 1;
    }

    static_cast<void>(central_zone_controller.RequestNodeHealth());
    static_cast<void>(central_zone_controller.SendLampCommand(
        body_control::lighting::domain::LampFunction::kParkLamp,
        body_control::lighting::domain::LampCommandAction::kActivate,
        body_control::lighting::domain::CommandSource::kCentralZoneController));
    static_cast<void>(central_zone_controller.RequestLampStatus(
        body_control::lighting::domain::LampFunction::kParkLamp));

    body_control::lighting::domain::LampStatus lamp_status {};
    if (central_zone_controller.GetCachedLampStatus(
            body_control::lighting::domain::LampFunction::kParkLamp,
            lamp_status))
    {
        std::cout << "Park lamp status output_state="
                  << static_cast<int>(lamp_status.output_state)
                  << ", applied="
                  << (lamp_status.command_applied ? "true" : "false")
                  << ", sequence_counter="
                  << lamp_status.last_sequence_counter
                  << '\n';
    }

    const body_control::lighting::domain::NodeHealthStatus node_health_status =
        central_zone_controller.GetCachedNodeHealthStatus();

    std::cout << "Rear node available="
              << (central_zone_controller.IsRearNodeAvailable() ? "true" : "false")
              << ", node_state="
              << static_cast<int>(node_health_status.node_state)
              << '\n';

    static_cast<void>(central_zone_controller.Shutdown());
    return 0;
}