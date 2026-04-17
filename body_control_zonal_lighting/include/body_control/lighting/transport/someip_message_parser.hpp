#ifndef BODY_CONTROL_LIGHTING_TRANSPORT_SOMEIP_MESSAGE_PARSER_HPP
#define BODY_CONTROL_LIGHTING_TRANSPORT_SOMEIP_MESSAGE_PARSER_HPP

#include "body_control/lighting/domain/lamp_command_types.hpp"
#include "body_control/lighting/domain/lamp_status_types.hpp"
#include "body_control/lighting/transport/transport_adapter_interface.hpp"

namespace body_control
{
namespace lighting
{
namespace transport
{

class SomeipMessageParser
{
public:
    static bool IsSetLampCommandRequest(
        const TransportMessage& transport_message) noexcept;

    static bool IsGetLampStatusRequest(
        const TransportMessage& transport_message) noexcept;

    static bool IsGetNodeHealthRequest(
        const TransportMessage& transport_message) noexcept;

    static bool IsLampStatusEvent(
        const TransportMessage& transport_message) noexcept;

    static bool IsNodeHealthEvent(
        const TransportMessage& transport_message) noexcept;

    static domain::LampCommand ParseLampCommand(
        const TransportMessage& transport_message);

    static domain::LampFunction ParseLampFunction(
        const TransportMessage& transport_message);

    static domain::LampStatus ParseLampStatus(
        const TransportMessage& transport_message);

    static domain::NodeHealthStatus ParseNodeHealthStatus(
        const TransportMessage& transport_message);

    // Operator service predicates.
    [[nodiscard]] static bool IsOperatorLampToggleRequest(
        const TransportMessage& transport_message) noexcept;

    [[nodiscard]] static bool IsOperatorLampActivateRequest(
        const TransportMessage& transport_message) noexcept;

    [[nodiscard]] static bool IsOperatorLampDeactivateRequest(
        const TransportMessage& transport_message) noexcept;

    [[nodiscard]] static bool IsOperatorNodeHealthRequest(
        const TransportMessage& transport_message) noexcept;

    [[nodiscard]] static bool IsOperatorLampStatusEvent(
        const TransportMessage& transport_message) noexcept;

    [[nodiscard]] static bool IsOperatorNodeHealthEvent(
        const TransportMessage& transport_message) noexcept;

    // Operator service parsers (reuse ParseLampFunction / ParseLampStatus /
    // ParseNodeHealthStatus — payload layout is identical).
};

}  // namespace transport
}  // namespace lighting
}  // namespace body_control

#endif  // BODY_CONTROL_LIGHTING_TRANSPORT_SOMEIP_MESSAGE_PARSER_HPP