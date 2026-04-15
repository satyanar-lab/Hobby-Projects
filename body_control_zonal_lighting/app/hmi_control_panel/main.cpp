#include <iostream>
#include <memory>

#include "body_control/lighting/application/central_zone_controller.hpp"
#include "body_control/lighting/domain/lamp_status_types.hpp"
#include "body_control/lighting/hmi/hmi_command_mapper.hpp"
#include "body_control/lighting/hmi/main_window.hpp"
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
    std::cout << "\n=== HMI Control Panel ===\n";
    std::cout << "1 -> Toggle left indicator\n";
    std::cout << "2 -> Toggle right indicator\n";
    std::cout << "3 -> Toggle hazard lamp\n";
    std::cout << "4 -> Toggle park lamp\n";
    std::cout << "5 -> Toggle head lamp\n";
    std::cout << "6 -> Request node health\n";
    std::cout << "0 -> Exit\n";
    std::cout << "Selection: ";
}

void RefreshViewModelFromController(
    body_control::lighting::application::CentralZoneController& central_zone_controller,
    body_control::lighting::hmi::MainWindow& main_window)
{
    using body_control::lighting::domain::LampFunction;
    using body_control::lighting::domain::LampStatus;

    constexpr LampFunction kLampFunctions[] = {
        LampFunction::kLeftIndicator,
        LampFunction::kRightIndicator,
        LampFunction::kHazardLamp,
        LampFunction::kParkLamp,
        LampFunction::kHeadLamp};

    for (const LampFunction lamp_function : kLampFunctions)
    {
        LampStatus lamp_status {};
        if (central_zone_controller.GetCachedLampStatus(lamp_function, lamp_status))
        {
            main_window.UpdateLampStatus(lamp_status);
        }
    }

    main_window.UpdateNodeHealthStatus(
        central_zone_controller.GetCachedNodeHealthStatus());
}

void PrintViewModel(
    const body_control::lighting::hmi::HmiViewModel& view_model)
{
    using body_control::lighting::domain::LampFunction;
    using body_control::lighting::domain::LampStatus;
    using body_control::lighting::domain::NodeHealthStatus;

    constexpr LampFunction kLampFunctions[] = {
        LampFunction::kLeftIndicator,
        LampFunction::kRightIndicator,
        LampFunction::kHazardLamp,
        LampFunction::kParkLamp,
        LampFunction::kHeadLamp};

    std::cout << "\n--- Lamp Status ---\n";
    for (const LampFunction lamp_function : kLampFunctions)
    {
        LampStatus lamp_status {};
        if (view_model.GetLampStatus(lamp_function, lamp_status))
        {
            std::cout << "Function "
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

    const NodeHealthStatus node_health_status =
        view_model.GetNodeHealthStatus();

    std::cout << "--- Node Health ---\n";
    std::cout << "node_state="
              << static_cast<int>(node_health_status.node_state)
              << ", ethernet_link_up="
              << (node_health_status.ethernet_link_up ? "true" : "false")
              << ", service_available="
              << (node_health_status.service_available ? "true" : "false")
              << ", sequence_counter="
              << node_health_status.last_sequence_counter
              << '\n';
}

}  // namespace

int main()
{
    using body_control::lighting::application::CentralZoneController;
    using body_control::lighting::application::ControllerStatus;
    using body_control::lighting::hmi::HmiAction;
    using body_control::lighting::hmi::HmiCommandMapper;
    using body_control::lighting::hmi::MainWindow;
    using body_control::lighting::hmi::MainWindowStatus;
    using body_control::lighting::service::RearLightingServiceConsumer;
    using body_control::lighting::transport::TransportAdapterInterface;

    std::unique_ptr<TransportAdapterInterface> transport_adapter =
        body_control::lighting::transport::vsomeip::
            CreateCentralZoneControllerVsomeipClientAdapter();

    if (transport_adapter == nullptr)
    {
        std::cerr << "Failed to create HMI transport adapter.\n";
        return 1;
    }

    RearLightingServiceConsumer rear_lighting_service_consumer {
        *transport_adapter};

    CentralZoneController central_zone_controller {
        rear_lighting_service_consumer};

    MainWindow main_window {central_zone_controller};

    const ControllerStatus init_status =
        central_zone_controller.Initialize();

    if (init_status != ControllerStatus::kSuccess)
    {
        std::cerr << "Failed to initialize HMI control panel.\n";
        return 1;
    }

    bool keep_running {true};

    while (keep_running)
    {
        PrintMenu();

        char input_key {'0'};
        std::cin >> input_key;

        if (input_key == '0')
        {
            keep_running = false;
            continue;
        }

        const HmiAction action =
            HmiCommandMapper::MapInputToAction(input_key);

        const MainWindowStatus window_status =
            main_window.ProcessAction(action);

        RefreshViewModelFromController(
            central_zone_controller,
            main_window);

        if (window_status != MainWindowStatus::kSuccess)
        {
            std::cout << "Action failed.\n";
            continue;
        }

        PrintViewModel(main_window.GetViewModel());
    }

    const ControllerStatus shutdown_status =
        central_zone_controller.Shutdown();

    if (shutdown_status != ControllerStatus::kSuccess)
    {
        std::cerr << "HMI control panel shutdown completed with errors.\n";
        return 1;
    }

    return 0;
}