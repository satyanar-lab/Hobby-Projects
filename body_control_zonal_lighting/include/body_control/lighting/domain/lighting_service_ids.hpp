#ifndef BODY_CONTROL_LIGHTING_DOMAIN_LIGHTING_SERVICE_IDS_HPP
#define BODY_CONTROL_LIGHTING_DOMAIN_LIGHTING_SERVICE_IDS_HPP

#include <cstdint>

namespace body_control
{
namespace lighting
{
namespace domain
{

/**
 * SOME/IP identifiers for the Rear Lighting Service.
 *
 * This service sits between the Central Zone Controller (provider) and the
 * Rear Lighting Node (consumer).  The controller issues lamp commands as
 * method calls; the node fires lamp-state and health events back to all
 * subscribers.
 *
 * ID allocation convention used throughout this project:
 *   0x5xxx  — service IDs
 *   0x0001  — first instance of a service (no multi-instance needed here)
 *   0x0xxx  — method IDs (request/response)
 *   0x8xxx  — event IDs (fire-and-forget notifications, MSB set per SOME/IP spec)
 *   0x1xxx  — application (client) IDs
 *   0x2xxx  — application IDs for simulated ECU nodes
 */
namespace rear_lighting_service
{

/// SOME/IP service identifier for the rear lighting service.
constexpr std::uint16_t kServiceId {0x5100U};

/// Only one instance of the rear lighting service exists on the bus.
constexpr std::uint16_t kInstanceId {0x0001U};

/// Method call: controller sends a LampCommand payload to the rear node.
constexpr std::uint16_t kSetLampCommandMethodId {0x0001U};

/// Method call: controller requests the current LampStatus for one function.
constexpr std::uint16_t kGetLampStatusMethodId {0x0002U};

/// Method call: controller requests a full NodeHealthStatus snapshot.
constexpr std::uint16_t kGetNodeHealthMethodId {0x0003U};

/// Event: rear node pushes LampStatus at kLampStatusPublishPeriod (100 ms).
/// MSB set (0x8xxx) per SOME/IP spec to distinguish events from methods.
constexpr std::uint16_t kLampStatusEventId {0x8001U};

/// Event: rear node pushes NodeHealthStatus at kNodeHealthPublishPeriod (1 s).
constexpr std::uint16_t kNodeHealthEventId {0x8002U};

/// Event group covering the lamp-status event; subscribers use this to
/// request notification delivery from the service provider.
constexpr std::uint16_t kLampStatusEventGroupId {0x0001U};

/// Event group covering the node-health event.
constexpr std::uint16_t kNodeHealthEventGroupId {0x0002U};

/// vsomeip application ID for the Central Zone Controller process.
constexpr std::uint16_t kCentralZoneControllerApplicationId {0x1001U};

/// vsomeip application ID for the HMI Control Panel process.
constexpr std::uint16_t kHmiControlPanelApplicationId {0x1002U};

/// vsomeip application ID for the Rear Lighting Node simulator process.
constexpr std::uint16_t kRearLightingNodeSimulatorApplicationId {0x2001U};

}  // namespace rear_lighting_service

/**
 * SOME/IP identifiers for the Operator Service.
 *
 * This service sits between the HMI / Diagnostic Console (consumer) and the
 * Central Zone Controller (provider).  Operator actions (toggle, activate,
 * deactivate) arrive as method calls; the controller fans lamp state and
 * health updates back to all connected operator clients as events.
 *
 * On the Linux loopback simulation the controller and operator clients
 * share the same host.  Dedicated UDP ports are assigned so vsomeip can
 * demultiplex traffic between the two directions without relying on
 * multicast routing in the WSL2 network stack.
 *
 * Port assignments:
 *   kControllerOperatorPort  41002  — controller's receive socket
 *   kOperatorClientPort      41003  — HMI / diagnostic console receive socket
 */
namespace operator_service
{

/// SOME/IP service identifier for the operator service.
constexpr std::uint16_t kServiceId {0x5200U};

/// Single instance; only one Central Zone Controller runs per vehicle zone.
constexpr std::uint16_t kInstanceId {0x0001U};

/// Method: request a toggle on the specified lamp (HMI sends this on button press).
constexpr std::uint16_t kRequestLampToggleMethodId {0x0001U};

/// Method: explicitly activate a specific lamp function.
constexpr std::uint16_t kRequestLampActivateMethodId {0x0002U};

/// Method: explicitly deactivate a specific lamp function.
constexpr std::uint16_t kRequestLampDeactivateMethodId {0x0003U};

/// Method: request a fresh NodeHealthStatus snapshot from the controller.
constexpr std::uint16_t kRequestNodeHealthMethodId {0x0004U};

/// Event: controller pushes LampStatus updates to all operator subscribers.
constexpr std::uint16_t kLampStatusEventId {0x8001U};

/// Event: controller pushes NodeHealthStatus updates to all operator subscribers.
constexpr std::uint16_t kNodeHealthEventId {0x8002U};

/// vsomeip application ID for the HMI Control Panel on the operator bus.
constexpr std::uint16_t kHmiControlPanelApplicationId {0x1003U};

/// vsomeip application ID for the Diagnostic Console on the operator bus.
constexpr std::uint16_t kDiagnosticConsoleApplicationId {0x1004U};

/// vsomeip application ID for the controller's operator-facing endpoint.
constexpr std::uint16_t kControllerOperatorApplicationId {0x1005U};

/// UDP port on which the controller listens for incoming operator requests.
constexpr std::uint16_t kControllerOperatorPort {41002U};

/// UDP port on which HMI / diagnostic-console clients listen for event pushes.
constexpr std::uint16_t kOperatorClientPort {41003U};

}  // namespace operator_service

}  // namespace domain
}  // namespace lighting
}  // namespace body_control

#endif  // BODY_CONTROL_LIGHTING_DOMAIN_LIGHTING_SERVICE_IDS_HPP
