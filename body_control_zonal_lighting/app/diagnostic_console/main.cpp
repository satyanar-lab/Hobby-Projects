#include <iostream>
#include <string>

#include "body_control/lighting/application/central_zone_controller.hpp"
#include "body_control/lighting/domain/lighting_types.hpp"
#include "body_control/lighting/platform/linux/linux_diagnostic_logger.hpp"
#include "body_control/lighting/service/rear_lighting_service_consumer.hpp"
#include "body_control/lighting/transport/vsomeip/vsomeip_client_adapter.hpp"
#include "body_control/lighting/transport/vsomeip/vsomeip_runtime_manager.hpp"

namespace
{

[[nodiscard]] bool TryParseLampFunction(
    const std::string& command_text,
    body_control::lighting::domain::LampFunction& function) noexcept
{
    bool parse_success {true};

    if (command_text == "left")
    {
        function = body_control::lighting::domain::LampFunction::kLeftIndicator;
    }
    else if (command_text == "right")
    {
        function = body_control::lighting::domain::LampFunction::kRightIndicator;
    }
    else if (command_text == "hazard")
    {
        function = body_control::lighting::domain::LampFunction::kHazardLamp;
    }
    else if (command_text == "park")
    {
        function = body_control::lighting::domain::LampFunction::kParkLamp;
    }
    else if (command_text == "head")
    {
        function = body_control::lighting::domain::LampFunction::kHeadLamp;
    }
    else
    {
        parse_success = false;
    }

    return parse_success;
}

void PrintHelp()
{
    std::cout << "Commands:\n"
              << "  on <left|right|hazard|park|head>\n"
              << "  off <left|right|hazard|park|head>\n"
              << "  status <left|right|hazard|park|head>\n"
              << "  health\n"
              << "  help\n"
              << "  quit\n";
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

    rear_lighting_service.SetEventListener(&central_zone_controller);

    if (rear_lighting_service.Initialize() != service::ServiceStatus::kSuccess)
    {
        diagnostic_logger.LogError("Failed to initialize transport/service layer");
        return 1;
    }

    if (!central_zone_controller.Initialize())
    {
        diagnostic_logger.LogError("Failed to initialize central zone controller");
        static_cast<void>(rear_lighting_service.Shutdown());
        return 1;
    }

    diagnostic_logger.LogInfo("Diagnostic console ready");
    PrintHelp();

    std::string verb {};
    while (true)
    {
        std::cout << "\ncmd> ";
        std::cin >> verb;

        if (!std::cin.good())
        {
            break;
        }

        if (verb == "quit")
        {
            break;
        }

        if (verb == "help")
        {
            PrintHelp();
            continue;
        }

        if (verb == "health")
        {
            const application::ControllerStatus controller_status =
                central_zone_controller.RequestNodeHealth();

            std::cout << "health request result: "
                      << static_cast<int>(controller_status)
                      << '\n';
            continue;
        }

        if ((verb == "on") || (verb == "off") || (verb == "status"))
        {
            std::string lamp_name {};
            std::cin >> lamp_name;

            domain::LampFunction function {domain::LampFunction::kUnknown};
            if (!TryParseLampFunction(lamp_name, function))
            {
                std::cout << "invalid lamp name\n";
                continue;
            }

            if (verb == "status")
            {
                const application::ControllerStatus controller_status =
                    central_zone_controller.RequestLampStatus(function);

                std::cout << "status request result: "
                          << static_cast<int>(controller_status)
                          << '\n';
                continue;
            }

            const domain::LampCommandAction action =
                (verb == "on")
                    ? domain::LampCommandAction::kActivate
                    : domain::LampCommandAction::kDeactivate;

            const application::ControllerStatus controller_status =
                central_zone_controller.SendLampCommand(
                    function,
                    action,
                    domain::CommandSource::kDiagnostic);

            std::cout << "command result: "
                      << static_cast<int>(controller_status)
                      << '\n';
            continue;
        }

        std::cout << "unknown command\n";
    }

    static_cast<void>(rear_lighting_service.Shutdown());

    return 0;
}