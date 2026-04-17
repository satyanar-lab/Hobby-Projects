#include <iostream>
#include <memory>

#include "body_control/lighting/domain/lamp_status_types.hpp"
#include "body_control/lighting/domain/lighting_service_ids.hpp"
#include "body_control/lighting/hmi/hmi_command_mapper.hpp"
#include "body_control/lighting/hmi/hmi_display_strings.hpp"
#include "body_control/lighting/hmi/main_window.hpp"
#include "body_control/lighting/service/operator_service_consumer.hpp"
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
CreateOperatorClientVsomeipClientAdapter();

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

void PrintViewModel(
    const body_control::lighting::hmi::HmiViewModel& view_model)
{
    using body_control::lighting::domain::LampFunction;
    using body_control::lighting::domain::LampStatus;
    using body_control::lighting::domain::NodeHealthStatus;
    using body_control::lighting::hmi::LampFunctionToString;
    using body_control::lighting::hmi::LampOutputStateToString;
    using body_control::lighting::hmi::NodeHealthStateToString;

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
            std::cout << LampFunctionToString(lamp_status.function)
                      << " -> "
                      << LampOutputStateToString(lamp_status.output_state)
                      << ", applied="
                      << (lamp_status.command_applied ? "true" : "false")
                      << ", seq="
                      << lamp_status.last_sequence_counter
                      << '\n';
        }
    }

    const NodeHealthStatus node_health_status =
        view_model.GetNodeHealthStatus();

    std::cout << "--- Node Health ---\n";
    std::cout << NodeHealthStateToString(node_health_status.health_state)
              << ", eth="
              << (node_health_status.ethernet_link_available ? "up" : "down")
              << ", svc="
              << (node_health_status.service_available ? "up" : "down")
              << ", fault="
              << (node_health_status.lamp_driver_fault_present ? "yes" : "no")
              << ", fault_count="
              << node_health_status.active_fault_count
              << '\n';
}

}  // namespace

int main()
{
    using body_control::lighting::domain::operator_service::
        kHmiControlPanelApplicationId;
    using body_control::lighting::hmi::HmiAction;
    using body_control::lighting::hmi::HmiCommandMapper;
    using body_control::lighting::hmi::MainWindow;
    using body_control::lighting::hmi::MainWindowStatus;
    using body_control::lighting::service::OperatorServiceConsumer;
    using body_control::lighting::service::OperatorServiceStatus;
    using body_control::lighting::transport::TransportAdapterInterface;

    std::unique_ptr<TransportAdapterInterface> transport_adapter =
        body_control::lighting::transport::vsomeip::
            CreateOperatorClientVsomeipClientAdapter();

    if (transport_adapter == nullptr)
    {
        std::cerr << "Failed to create HMI transport adapter.\n";
        return 1;
    }

    OperatorServiceConsumer operator_service {
        *transport_adapter,
        kHmiControlPanelApplicationId};

    MainWindow main_window {operator_service};
    operator_service.SetEventListener(&main_window);

    const OperatorServiceStatus init_status =
        operator_service.Initialize();

    if (init_status != OperatorServiceStatus::kSuccess)
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

        if (window_status != MainWindowStatus::kSuccess)
        {
            std::cout << "Action failed.\n";
            continue;
        }

        PrintViewModel(main_window.GetViewModel());
    }

    const OperatorServiceStatus shutdown_status =
        operator_service.Shutdown();

    if (shutdown_status != OperatorServiceStatus::kSuccess)
    {
        std::cerr << "HMI control panel shutdown completed with errors.\n";
        return 1;
    }

    return 0;
}
