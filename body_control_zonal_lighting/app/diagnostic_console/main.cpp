#include <iostream>
#include <memory>

#include "body_control/lighting/application/central_zone_controller.hpp"
#include "body_control/lighting/domain/lamp_command_types.hpp"
#include "body_control/lighting/domain/lamp_status_types.hpp"
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

namespace
{

void PrintMenu()
{
    std::cout << "\n=== Diagnostic Console ===\n";
    std::cout << "1  -> Activate left indicator\n";
    std::cout << "2  -> Deactivate left indicator\n";
    std::cout << "3  -> Activate right indicator\n";
    std::cout << "4  -> Deactivate right indicator\n";
    std::cout << "5  -> Activate hazard lamp\n";
    std::cout << "6  -> Deactivate hazard lamp\n";
    std::cout << "7  -> Activate park lamp\n";
    std::cout << "8  -> Deactivate park lamp\n";
    std::cout << "9  -> Activate head lamp\n";
    std::cout << "10 -> Deactivate head lamp\n";
    std::cout << "11 -> Request node health\n";
    std::cout << "0  -> Exit\n";
    std::cout << "Selection: ";
}

void PrintLampStatus(
    body_control::lighting::application::CentralZoneController& central_zone_controller,
    const body_control::lighting::domain::LampFunction lamp_function)
{
    body_control::lighting::domain::LampStatus lamp_status {};

    if (central_zone_controller.GetCachedLampStatus(lamp_function, lamp_status))
    {
        std::cout << "Lamp function "
                  << static_cast<int>(lamp_status.function)
                  << " -> output_state="
                  << static_cast<int>(lamp_status.output_state)
                  << ", applied="
                  << (lamp_status.command_applied ? "true" : "false")
                  << ", sequence_counter="
                  << lamp_status.last_sequence_counter
                  << '\n';
    }
}

}  // namespace

int main()
{
    using body_control::lighting::application::CentralZoneController;
    using body_control::lighting::application::ControllerStatus;
    using body_control::lighting::domain::CommandSource;
    using body_control::lighting::domain::LampCommandAction;
    using body_control::lighting::domain::LampFunction;
    using body_control::lighting::service::RearLightingServiceConsumer;
    using body_control::lighting::transport::TransportAdapterInterface;

    std::unique_ptr<TransportAdapterInterface> transport_adapter =
        body_control::lighting::transport::vsomeip::
            CreateCentralZoneControllerVsomeipClientAdapter();

    if (transport_adapter == nullptr)
    {
        std::cerr << "Failed to create diagnostic transport adapter.\n";
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
        std::cerr << "Failed to initialize diagnostic console.\n";
        return 1;
    }

    bool keep_running {true};

    while (keep_running)
    {
        PrintMenu();

        int selection {0};
        std::cin >> selection;

        ControllerStatus controller_status {ControllerStatus::kSuccess};
        LampFunction status_function_to_print {LampFunction::kUnknown};
        bool should_print_lamp_status {false};

        switch (selection)
        {
        case 1:
            controller_status = central_zone_controller.SendLampCommand(
                LampFunction::kLeftIndicator,
                LampCommandAction::kActivate,
                CommandSource::kDiagnosticConsole);
            status_function_to_print = LampFunction::kLeftIndicator;
            should_print_lamp_status = true;
            break;

        case 2:
            controller_status = central_zone_controller.SendLampCommand(
                LampFunction::kLeftIndicator,
                LampCommandAction::kDeactivate,
                CommandSource::kDiagnosticConsole);
            status_function_to_print = LampFunction::kLeftIndicator;
            should_print_lamp_status = true;
            break;

        case 3:
            controller_status = central_zone_controller.SendLampCommand(
                LampFunction::kRightIndicator,
                LampCommandAction::kActivate,
                CommandSource::kDiagnosticConsole);
            status_function_to_print = LampFunction::kRightIndicator;
            should_print_lamp_status = true;
            break;

        case 4:
            controller_status = central_zone_controller.SendLampCommand(
                LampFunction::kRightIndicator,
                LampCommandAction::kDeactivate,
                CommandSource::kDiagnosticConsole);
            status_function_to_print = LampFunction::kRightIndicator;
            should_print_lamp_status = true;
            break;

        case 5:
            controller_status = central_zone_controller.SendLampCommand(
                LampFunction::kHazardLamp,
                LampCommandAction::kActivate,
                CommandSource::kDiagnosticConsole);
            status_function_to_print = LampFunction::kHazardLamp;
            should_print_lamp_status = true;
            break;

        case 6:
            controller_status = central_zone_controller.SendLampCommand(
                LampFunction::kHazardLamp,
                LampCommandAction::kDeactivate,
                CommandSource::kDiagnosticConsole);
            status_function_to_print = LampFunction::kHazardLamp;
            should_print_lamp_status = true;
            break;

        case 7:
            controller_status = central_zone_controller.SendLampCommand(
                LampFunction::kParkLamp,
                LampCommandAction::kActivate,
                CommandSource::kDiagnosticConsole);
            status_function_to_print = LampFunction::kParkLamp;
            should_print_lamp_status = true;
            break;

        case 8:
            controller_status = central_zone_controller.SendLampCommand(
                LampFunction::kParkLamp,
                LampCommandAction::kDeactivate,
                CommandSource::kDiagnosticConsole);
            status_function_to_print = LampFunction::kParkLamp;
            should_print_lamp_status = true;
            break;

        case 9:
            controller_status = central_zone_controller.SendLampCommand(
                LampFunction::kHeadLamp,
                LampCommandAction::kActivate,
                CommandSource::kDiagnosticConsole);
            status_function_to_print = LampFunction::kHeadLamp;
            should_print_lamp_status = true;
            break;

        case 10:
            controller_status = central_zone_controller.SendLampCommand(
                LampFunction::kHeadLamp,
                LampCommandAction::kDeactivate,
                CommandSource::kDiagnosticConsole);
            status_function_to_print = LampFunction::kHeadLamp;
            should_print_lamp_status = true;
            break;

        case 11:
        {
            controller_status = central_zone_controller.RequestNodeHealth();

            const body_control::lighting::domain::NodeHealthStatus
                node_health_status =
                    central_zone_controller.GetCachedNodeHealthStatus();

            std::cout << "Node health: health_state="
                      << static_cast<int>(node_health_status.health_state)
                      << ", ethernet_link_available="
                      << (node_health_status.ethernet_link_available ? "true" : "false")
                      << ", service_available="
                      << (node_health_status.service_available ? "true" : "false")
                      << ", lamp_driver_fault_present="
                      << (node_health_status.lamp_driver_fault_present ? "true" : "false")
                      << ", active_fault_count="
                      << node_health_status.active_fault_count
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

        if (controller_status != ControllerStatus::kSuccess)
        {
            std::cout << "Command failed.\n";
            continue;
        }

        if (should_print_lamp_status)
        {
            PrintLampStatus(
                central_zone_controller,
                status_function_to_print);
        }
    }

    const ControllerStatus shutdown_status =
        central_zone_controller.Shutdown();

    if (shutdown_status != ControllerStatus::kSuccess)
    {
        std::cerr << "Diagnostic console shutdown completed with errors.\n";
        return 1;
    }

    return 0;
}