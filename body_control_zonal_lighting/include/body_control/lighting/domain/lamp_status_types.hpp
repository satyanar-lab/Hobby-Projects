#ifndef BODY_CONTROL_LIGHTING_DOMAIN_LAMP_STATUS_TYPES_HPP
#define BODY_CONTROL_LIGHTING_DOMAIN_LAMP_STATUS_TYPES_HPP

#include <cstdint>

#include "body_control/lighting/domain/lamp_command_types.hpp"

namespace body_control
{
namespace lighting
{
namespace domain
{

enum class LampOutputState : std::uint8_t
{
    kUnknown = 0U,
    kOff = 1U,
    kOn = 2U
};

struct LampStatus
{
    LampFunction function {LampFunction::kUnknown};
    LampOutputState output_state {LampOutputState::kUnknown};
    bool command_applied {false};
    std::uint32_t last_sequence_counter {0U};
};

enum class NodeHealthState : std::uint8_t
{
    kUnknown = 0U,
    kOperational = 1U,
    kDegraded = 2U,
    kFaulted = 3U,
    kOffline = 4U
};

struct NodeHealthStatus
{
    NodeHealthState node_state {NodeHealthState::kUnknown};
    bool ethernet_link_up {false};
    bool service_available {false};
    std::uint32_t last_sequence_counter {0U};
};

}  // namespace domain
}  // namespace lighting
}  // namespace body_control

#endif  // BODY_CONTROL_LIGHTING_DOMAIN_LAMP_STATUS_TYPES_HPP