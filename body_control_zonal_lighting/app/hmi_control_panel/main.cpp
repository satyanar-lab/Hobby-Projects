#include <iostream>
#include <string>

#include "body_control/lighting/application/central_zone_controller.hpp"
#include "body_control/lighting/domain/lighting_types.hpp"
#include "body_control/lighting/hmi/main_window.hpp"
#include "body_control/lighting/platform/linux/linux_diagnostic_logger.hpp"
#include "body_control/lighting/service/rear_lighting_service_consumer.hpp"
#include "body_control/lighting/transport/vsomeip/vsomeip_client_adapter.hpp"
#include "body_control/lighting/transport/vsomeip/vsomeip_runtime_manager.hpp"

namespace
{

using body_control::lighting::domain::LampFunction;
using body_control::lighting::domain::LampOutputState;
using body_control::lighting::hmi::HmiAction;

[[nodiscard]] bool TryParseAction(
    const std::string& command_text,
    HmiAction& action,
    LampFunction& function) noexcept
{
    bool parse_success {true};

    if (command_text == "left")
    {
        action = HmiAction::kToggleLeftIndicator;
        function = LampFunction::kLeftIndicator;
    }
    else if (command_text == "right")
    {
        action = HmiAction::kToggleRightIndicator;
        function = LampFunction::kRightIndicator;
    }
    else if (command_text == "hazard")
    {
        action = HmiAction::kToggleHazardLamp;
        function = LampFunction::kHazardLamp;
    }
    else if (command_text == "park")
    {
        action = HmiAction::kToggleParkLamp;
        function = LampFunction::kParkLamp;
    }
    else if (command_text == "head")
    {
        action = HmiAction::kToggleHeadLamp;
        function = LampFunction::kHeadLamp;
    }
    else
    {
        parse_success = false;
    }

    return parse_success;
}

void PrintMenu()
{
    std::cout << "\nHMI commands:\n"
              << "  left\n"
              << "  right\n"
              << "  hazard\n"
              << "  park\n"
              << "  head\n"
              << "  health\n"
              << "  state\n"
              << "  quit\n";
}

void PrintCurrentState(const body_control::lighting::hmi::MainWindow& main_window)
{
    const auto& view_model = main_window.GetViewModel();

    std::cout << "\nCurrent state\n"
              << "  left   : " << view_model.IsLampFunctionActive(LampFunction::kLeftIndicator) << '\n'
              << "  right  : " << view_model.IsLampFunctionActive(LampFunction::kRightIndicator) << '\n'
              << "  hazard : " << view_model.IsLampFunctionActive(LampFunction::kHazardLamp) << '\n'
              << "  park   : " << view_model.IsLampFunctionActive(LampFunction::kParkLamp) << '\n'
              << "  head   : " << view_model.IsLampFunctionActive(LampFunction::kHeadLamp) << '\n';

    const auto node_health = view_model.GetNodeHealthStatus();
    std::cout << "  node health state : " << static_cast<int>(node_health.health_state) << '\n'
              << "  ethernet link     : " << node_health.ethernet_link_available << '\n'
              << "  service available : " << node_health.service_available << '\n'
              << "  fault present     : " << node_health.lamp_driver_fault_present << '\n'
              << "  active faults     : " << static_cast<int>(node_health.active_fault_count) << '\n';
}

}  // namespace

int main()
{
    using namespace body_control::lighting;

    platform::linux::LinuxDiagnosticLogger diagnostic_logger {};
    static_cast<void>(diagnostic_logger.Initialize());

    transport::vsomeip::VsomeipRuntimeManager runtime_manager {};
    transport::vsomeip::VsomeipClientAdapter transport_adapter {runtime_manager};
    service::RearLightingServiceConsumer rear_lighting_service {transport_adapter};
    application::CentralZoneController central_zone_controller {
        rear_lighting_service};
    hmi::MainWindow main_window {central_zone_controller};

    rear_lighting_service.SetEventListener(&central_zone_controller);

    if (rear_lighting_service.Initialize() != service::ServiceStatus::kSuccess)
    {
        diagnostic_logger.LogError("Failed to initialize rear lighting service");
        return 1;
    }

    if (!central_zone_controller.Initialize())
    {
        diagnostic_logger.LogError("Failed to initialize central zone controller");
        static_cast<void>(rear_lighting_service.Shutdown());
        return 1;
    }

    diagnostic_logger.LogInfo("HMI control panel started");
    PrintMenu();

    std::string command_text {};
    while (true)
    {
        std::cout << "\nhmi> ";
        std::cin >> command_text;

        if (!std::cin.good())
        {
            break;
        }

        if (command_text == "quit")
        {
            break;
        }

        if (command_text == "state")
        {
            PrintCurrentState(main_window);
            continue;
        }

        if (command_text == "health")
        {
            const hmi::MainWindowStatus status =
                main_window.ProcessAction(HmiAction::kRequestNodeHealth);

            std::cout << "health request result: "
                      << static_cast<int>(status)
                      << '\n';
            continue;
        }

        HmiAction action {HmiAction::kUnknown};
        LampFunction function {LampFunction::kUnknown};

        if (!TryParseAction(command_text, action, function))
        {
            std::cout << "unknown command";
            continue;
        }

        const bool current_state =
            main_window.GetViewModel().IsLampFunctionActive(function);

        const hmi::MainWindowStatus status =
            main_window.ProcessAction(action);

        std::cout << "command result: "
                  << static_cast<int>(status)
                  << '\n';

        if (status == hmi::MainWindowStatus::kSuccess)
        {
            domain::LampStatus lamp_status {};
            lamp_status.function = function;
            lamp_status.output_state =
                current_state ? LampOutputState::kOff
                              : LampOutputState::kOn;
            lamp_status.current_draw_milliamps = 0U;
            lamp_status.driver_fault_present = false;

            main_window.UpdateLampStatus(lamp_status);
        }
    }

    static_cast<void>(rear_lighting_service.Shutdown());

    return 0;
}