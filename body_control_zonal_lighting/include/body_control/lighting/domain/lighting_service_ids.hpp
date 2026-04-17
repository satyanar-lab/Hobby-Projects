#ifndef BODY_CONTROL_LIGHTING_DOMAIN_LIGHTING_SERVICE_IDS_HPP
#define BODY_CONTROL_LIGHTING_DOMAIN_LIGHTING_SERVICE_IDS_HPP

#include <cstdint>

namespace body_control
{
namespace lighting
{
namespace domain
{
namespace rear_lighting_service
{

constexpr std::uint16_t kServiceId {0x5100U};
constexpr std::uint16_t kInstanceId {0x0001U};

constexpr std::uint16_t kSetLampCommandMethodId {0x0001U};
constexpr std::uint16_t kGetLampStatusMethodId {0x0002U};
constexpr std::uint16_t kGetNodeHealthMethodId {0x0003U};

constexpr std::uint16_t kLampStatusEventId {0x8001U};
constexpr std::uint16_t kNodeHealthEventId {0x8002U};

constexpr std::uint16_t kLampStatusEventGroupId {0x0001U};
constexpr std::uint16_t kNodeHealthEventGroupId {0x0002U};

constexpr std::uint16_t kCentralZoneControllerApplicationId {0x1001U};
constexpr std::uint16_t kHmiControlPanelApplicationId {0x1002U};
constexpr std::uint16_t kRearLightingNodeSimulatorApplicationId {0x2001U};

}  // namespace rear_lighting_service

// ----------------------------------------------------------------------------
// Operator service — HMI / diagnostic console ↔ central zone controller
//
// Port assignments (Linux loopback simulation):
//   kControllerOperatorPort  41002  controller-side receive socket
//   kOperatorClientPort      41003  HMI / diag-console receive socket
// ----------------------------------------------------------------------------
namespace operator_service
{

constexpr std::uint16_t kServiceId {0x5200U};
constexpr std::uint16_t kInstanceId {0x0001U};

constexpr std::uint16_t kRequestLampToggleMethodId {0x0001U};
constexpr std::uint16_t kRequestLampActivateMethodId {0x0002U};
constexpr std::uint16_t kRequestLampDeactivateMethodId {0x0003U};
constexpr std::uint16_t kRequestNodeHealthMethodId {0x0004U};

constexpr std::uint16_t kLampStatusEventId {0x8001U};
constexpr std::uint16_t kNodeHealthEventId {0x8002U};

constexpr std::uint16_t kHmiControlPanelApplicationId {0x1003U};
constexpr std::uint16_t kDiagnosticConsoleApplicationId {0x1004U};
constexpr std::uint16_t kControllerOperatorApplicationId {0x1005U};

constexpr std::uint16_t kControllerOperatorPort {41002U};
constexpr std::uint16_t kOperatorClientPort {41003U};

}  // namespace operator_service

}  // namespace domain
}  // namespace lighting
}  // namespace body_control

#endif  // BODY_CONTROL_LIGHTING_DOMAIN_LIGHTING_SERVICE_IDS_HPP