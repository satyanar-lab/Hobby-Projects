#include <iostream>
#include <string>

#include "body_control/lighting/application/central_zone_controller.hpp"
#include "body_control/lighting/domain/lighting_service_ids.hpp"
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

void PrintMenu()
{
    std::cout << "\n=== Diagnostic Console ===\n";
    std::cout << "1. Activate left indicator\n";
    std::cout << "2. Deactivate left indicator\n";
    std::cout << "3. Activate right indicator\n";
    std::cout << "4. Deactivate right indicator\n";
    std::cout << "5. Activate hazard lamp\n";
    std::cout << "6. Deactivate hazard lamp\n";
    std::cout << "7. Activate park lamp\n";
    std::cout << "8. Deactivate park lamp\n";
    std::cout << "9. Activate head lamp\n";
    std::cout << "10. Deactivate head lamp\n";
    std::cout << "11. Request node health\n";
    std::cout << "0. Exit\n";
    std::cout << "Selection: ";
}

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
        std::cerr << "Failed to initialize diagnostic console.\n";
        return 1;
    }

    bool keep_running {true};

    while (keep_running)
    {
        body_control::lighting::PrintMenu();

        int selection {0};
        std::cin >> selection;

        using body_control::lighting::application::ControllerStatus;
        using body_control::lighting::domain::CommandSource;
        using body_control::lighting::domain::LampCommandAction;
        using body_control::lighting::domain::LampFunction;

        ControllerStatus controller_status {ControllerStatus::kSuccess};

        switch (selection)
        {
        case 1:
            controller_status = central_zone_controller.SendLampCommand(
                LampFunction::kLeftIndicator,
                LampCommandAction::kActivate,
                CommandSource::kDiagnosticConsole);
            break;

        case 2:
            controller_status = central_zone_controller.SendLampCommand(
                LampFunction::kLeftIndicator,
                LampCommandAction::kDeactivate,
                CommandSource::kDiagnosticConsole);
            break;

        case 3:
            controller_status = central_zone_controller.SendLampCommand(
                LampFunction::kRightIndicator,
                LampCommandAction::kActivate,
                CommandSource::kDiagnosticConsole);
            break;

        case 4:
            controller_status = central_zone_controller.SendLampCommand(
                LampFunction::kRightIndicator,
                LampCommandAction::kDeactivate,
                CommandSource::kDiagnosticConsole);
            break;

        case 5:
            controller_status = central_zone_controller.SendLampCommand(
                LampFunction::kHazardLamp,
                LampCommandAction::kActivate,
                CommandSource::kDiagnosticConsole);
            break;

        case 6:
            controller_status = central_zone_controller.SendLampCommand(
                LampFunction::kHazardLamp,
                LampCommandAction::kDeactivate,
                CommandSource::kDiagnosticConsole);
            break;

        case 7:
            controller_status = central_zone_controller.SendLampCommand(
                LampFunction::kParkLamp,
                LampCommandAction::kActivate,
                CommandSource::kDiagnosticConsole);
            break;

        case 8:
            controller_status = central_zone_controller.SendLampCommand(
                LampFunction::kParkLamp,
                LampCommandAction::kDeactivate,
                CommandSource::kDiagnosticConsole);
            break;

        case 9:
            controller_status = central_zone_controller.SendLampCommand(
                LampFunction::kHeadLamp,
                LampCommandAction::kActivate,
                CommandSource::kDiagnosticConsole);
            break;

        case 10:
            controller_status = central_zone_controller.SendLampCommand(
                LampFunction::kHeadLamp,
                LampCommandAction::kDeactivate,
                CommandSource::kDiagnosticConsole);
            break;

        case 11:
        {
            controller_status = central_zone_controller.RequestNodeHealth();

            const body_control::lighting::domain::NodeHealthStatus
                node_health_status =
                    central_zone_controller.GetCachedNodeHealthStatus();

            std::cout << "Node health state: "
                      << static_cast<int>(node_health_status.node_state)
                      << ", ethernet_link_up="
                      << (node_health_status.ethernet_link_up ? "true" : "false")
                      << ", service_available="
                      << (node_health_status.service_available ? "true" : "false")
                      << '\n';
            break;
        }

        case 0:
            keep_running = false;
            continue;

        default:
            std::cout << "Invalid selection.\n";
            continue;
        }

        if (controller_status ==
            body_control::lighting::application::ControllerStatus::kSuccess)
        {
            std::cout << "Command processed.\n";
        }
        else
        {
            std::cout << "Command failed.\n";
        }
    }

    static_cast<void>(central_zone_controller.Shutdown());
    return 0;
}