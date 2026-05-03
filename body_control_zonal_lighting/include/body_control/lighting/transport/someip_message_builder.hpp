#ifndef BODY_CONTROL_LIGHTING_TRANSPORT_SOMEIP_MESSAGE_BUILDER_HPP
#define BODY_CONTROL_LIGHTING_TRANSPORT_SOMEIP_MESSAGE_BUILDER_HPP

#include "body_control/lighting/domain/fault_types.hpp"
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

    // Operator service — requests sent by HMI / diagnostic console.
    static TransportMessage BuildOperatorLampToggleRequest(
        domain::LampFunction lamp_function,
        std::uint16_t client_id,
        std::uint16_t session_id);

    static TransportMessage BuildOperatorLampActivateRequest(
        domain::LampFunction lamp_function,
        std::uint16_t client_id,
        std::uint16_t session_id);

    static TransportMessage BuildOperatorLampDeactivateRequest(
        domain::LampFunction lamp_function,
        std::uint16_t client_id,
        std::uint16_t session_id);

    static TransportMessage BuildOperatorNodeHealthRequest(
        std::uint16_t client_id,
        std::uint16_t session_id);

    // Operator service — events sent by the controller to operator clients.
    static TransportMessage BuildOperatorLampStatusEvent(
        const domain::LampStatus& lamp_status);

    static TransportMessage BuildOperatorNodeHealthEvent(
        const domain::NodeHealthStatus& node_health_status);

    // Rear lighting service — fault inject/clear requests sent by the controller.
    static TransportMessage BuildInjectFaultRequest(
        const domain::FaultCommand& fault_command,
        std::uint16_t client_id,
        std::uint16_t session_id);

    static TransportMessage BuildClearFaultRequest(
        const domain::FaultCommand& fault_command,
        std::uint16_t client_id,
        std::uint16_t session_id);

    static TransportMessage BuildGetFaultStatusRequest(
        std::uint16_t client_id,
        std::uint16_t session_id);

    // Rear lighting service — fault status event sent by the rear node.
    static TransportMessage BuildFaultStatusEvent(
        const domain::LampFaultStatus& fault_status);

    // Operator service — fault inject/clear requests sent by diagnostic console / HMI.
    static TransportMessage BuildOperatorInjectFaultRequest(
        domain::LampFunction lamp_function,
        std::uint16_t client_id,
        std::uint16_t session_id);

    static TransportMessage BuildOperatorClearFaultRequest(
        domain::LampFunction lamp_function,
        std::uint16_t client_id,
        std::uint16_t session_id);

    static TransportMessage BuildOperatorGetFaultStatusRequest(
        std::uint16_t client_id,
        std::uint16_t session_id);
};

}  // namespace transport
}  // namespace lighting
}  // namespace body_control

#endif  // BODY_CONTROL_LIGHTING_TRANSPORT_SOMEIP_MESSAGE_BUILDER_HPP