#ifndef BODY_CONTROL_LIGHTING_TRANSPORT_SOMEIP_MESSAGE_BUILDER_HPP
#define BODY_CONTROL_LIGHTING_TRANSPORT_SOMEIP_MESSAGE_BUILDER_HPP

#include "body_control/lighting/domain/lamp_command_types.hpp"
#include "body_control/lighting/domain/lamp_status_types.hpp"
#include "body_control/lighting/transport/transport_adapter_interface.hpp"

namespace body_control
{
namespace lighting
{
namespace transport
{

class SomeipMessageBuilder
{
public:
    static TransportMessage BuildSetLampCommandRequest(
        const domain::LampCommand& lamp_command,
        std::uint16_t client_id,
        std::uint16_t session_id);

    static TransportMessage BuildGetLampStatusRequest(
        domain::LampFunction lamp_function,
        std::uint16_t client_id,
        std::uint16_t session_id);

    static TransportMessage BuildGetNodeHealthRequest(
        std::uint16_t client_id,
        std::uint16_t session_id);

    static TransportMessage BuildLampStatusEvent(
        const domain::LampStatus& lamp_status);

    static TransportMessage BuildNodeHealthEvent(
        const domain::NodeHealthStatus& node_health_status);
};

}  // namespace transport
}  // namespace lighting
}  // namespace body_control

#endif  // BODY_CONTROL_LIGHTING_TRANSPORT_SOMEIP_MESSAGE_BUILDER_HPP