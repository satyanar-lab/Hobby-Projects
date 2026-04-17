#include <iostream>
#include <memory>

#include "body_control/lighting/domain/lamp_command_types.hpp"
#include "body_control/lighting/domain/lamp_status_types.hpp"
#include "body_control/lighting/domain/lighting_service_ids.hpp"
#include "body_control/lighting/hmi/hmi_display_strings.hpp"
#include "body_control/lighting/service/operator_service_consumer.hpp"
#include "body_control/lighting/service/operator_service_interface.hpp"
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

void PrintCachedLampStatus(
    const body_control::lighting::service::OperatorServiceConsumer& consumer,
    const body_control::lighting::domain::LampFunction lamp_function)
{
    using body_control::lighting::hmi::LampFunctionToString;
    using body_control::lighting::hmi::LampOutputStateToString;

    body_control::lighting::domain::LampStatus lamp_status {};

    if (consumer.GetLampStatus(lamp_function, lamp_status))
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

class DiagnosticEventListener final
    : public body_control::lighting::service::OperatorServiceEventListenerInterface
{
public:
    void OnLampStatusUpdated(
        const body_control::lighting::domain::LampStatus& lamp_status) override
    {
        using body_control::lighting::hmi::LampFunctionToString;
        using body_control::lighting::hmi::LampOutputStateToString;
        std::cout << "[event] "
                  << LampFunctionToString(lamp_status.function)
                  << " -> "
                  << LampOutputStateToString(lamp_status.output_state)
                  << '\n';
    }

    void OnNodeHealthUpdated(
        const body_control::lighting::domain::NodeHealthStatus&
            node_health_status) override
    {
        using body_control::lighting::hmi::NodeHealthStateToString;
        std::cout << "[event] node health: "
                  << NodeHealthStateToString(node_health_status.health_state)
                  << ", svc="
                  << (node_health_status.service_available ? "up" : "down")
                  << '\n';
    }

    void OnControllerAvailabilityChanged(const bool is_available) override
    {
        std::cout << "[event] controller "
                  << (is_available ? "available" : "unavailable")
                  << '\n';
    }
};

}  // namespace

int main()
{
    using body_control::lighting::domain::LampFunction;
    using body_control::lighting::domain::operator_service::
        kDiagnosticConsoleApplicationId;
    using body_control::lighting::service::OperatorServiceConsumer;
    using body_control::lighting::service::OperatorServiceStatus;
    using body_control::lighting::transport::TransportAdapterInterface;

    std::unique_ptr<TransportAdapterInterface> transport_adapter =
        body_control::lighting::transport::vsomeip::
            CreateOperatorClientVsomeipClientAdapter();

    if (transport_adapter == nullptr)
    {
        std::cerr << "Failed to create diagnostic transport adapter.\n";
        return 1;
    }

    OperatorServiceConsumer operator_service {
        *transport_adapter,
        kDiagnosticConsoleApplicationId};

    DiagnosticEventListener event_listener {};
    operator_service.SetEventListener(&event_listener);

    const OperatorServiceStatus init_status =
        operator_service.Initialize();

    if (init_status != OperatorServiceStatus::kSuccess)
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

        OperatorServiceStatus operator_status {OperatorServiceStatus::kSuccess};
        LampFunction status_function_to_print {LampFunction::kUnknown};
        bool should_print_lamp_status {false};

        switch (selection)
        {
        case 1:
            operator_status = operator_service.RequestLampActivate(
                LampFunction::kLeftIndicator);
            status_function_to_print = LampFunction::kLeftIndicator;
            should_print_lamp_status = true;
            break;

        case 2:
            operator_status = operator_service.RequestLampDeactivate(
                LampFunction::kLeftIndicator);
            status_function_to_print = LampFunction::kLeftIndicator;
            should_print_lamp_status = true;
            break;

        case 3:
            operator_status = operator_service.RequestLampActivate(
                LampFunction::kRightIndicator);
            status_function_to_print = LampFunction::kRightIndicator;
            should_print_lamp_status = true;
            break;

        case 4:
            operator_status = operator_service.RequestLampDeactivate(
                LampFunction::kRightIndicator);
            status_function_to_print = LampFunction::kRightIndicator;
            should_print_lamp_status = true;
            break;

        case 5:
            operator_status = operator_service.RequestLampActivate(
                LampFunction::kHazardLamp);
            status_function_to_print = LampFunction::kHazardLamp;
            should_print_lamp_status = true;
            break;

        case 6:
            operator_status = operator_service.RequestLampDeactivate(
                LampFunction::kHazardLamp);
            status_function_to_print = LampFunction::kHazardLamp;
            should_print_lamp_status = true;
            break;

        case 7:
            operator_status = operator_service.RequestLampActivate(
                LampFunction::kParkLamp);
            status_function_to_print = LampFunction::kParkLamp;
            should_print_lamp_status = true;
            break;

        case 8:
            operator_status = operator_service.RequestLampDeactivate(
                LampFunction::kParkLamp);
            status_function_to_print = LampFunction::kParkLamp;
            should_print_lamp_status = true;
            break;

        case 9:
            operator_status = operator_service.RequestLampActivate(
                LampFunction::kHeadLamp);
            status_function_to_print = LampFunction::kHeadLamp;
            should_print_lamp_status = true;
            break;

        case 10:
            operator_status = operator_service.RequestLampDeactivate(
                LampFunction::kHeadLamp);
            status_function_to_print = LampFunction::kHeadLamp;
            should_print_lamp_status = true;
            break;

        case 11:
        {
            operator_status = operator_service.RequestNodeHealth();

            body_control::lighting::domain::NodeHealthStatus node_health_status {};
            operator_service.GetNodeHealthStatus(node_health_status);

            using body_control::lighting::hmi::NodeHealthStateToString;
            std::cout << "Node health: "
                      << NodeHealthStateToString(node_health_status.health_state)
                      << ", eth="
                      << (node_health_status.ethernet_link_available ? "up" : "down")
                      << ", svc="
                      << (node_health_status.service_available ? "up" : "down")
                      << ", fault="
                      << (node_health_status.lamp_driver_fault_present ? "yes" : "no")
                      << ", fault_count="
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

        if (operator_status != OperatorServiceStatus::kSuccess)
        {
            std::cout << "Command failed.\n";
            continue;
        }

        if (should_print_lamp_status)
        {
            PrintCachedLampStatus(
                operator_service,
                status_function_to_print);
        }
    }

    const OperatorServiceStatus shutdown_status =
        operator_service.Shutdown();

    if (shutdown_status != OperatorServiceStatus::kSuccess)
    {
        std::cerr << "Diagnostic console shutdown completed with errors.\n";
        return 1;
    }

    return 0;
}
