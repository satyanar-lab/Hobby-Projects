#include "body_control/lighting/transport/someip_message_parser.hpp"

#include <cstddef>
#include <cstdint>

#include "body_control/lighting/domain/lighting_service_ids.hpp"

namespace body_control::lighting::transport
{
namespace
{

[[nodiscard]] std::uint8_t ReadUint8(
    const std::vector<std::uint8_t>& payload,
    const std::size_t index) noexcept
{
    if (index >= payload.size())
    {
        return 0U;
    }

    return payload[index];
}

[[nodiscard]] std::uint16_t ReadUint16(
    const std::vector<std::uint8_t>& payload,
    const std::size_t index) noexcept
{
    if ((index + 1U) >= payload.size())
    {
        return 0U;
    }

    return static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(payload[index]) << 8U) |
        static_cast<std::uint16_t>(payload[index + 1U]));
}

[[nodiscard]] bool IsRearLightingMessage(
    const TransportMessage& transport_message) noexcept
{
    return (transport_message.service_id ==
            domain::rear_lighting_service::kServiceId) &&
           (transport_message.instance_id ==
            domain::rear_lighting_service::kInstanceId);
}

}  // namespace

bool SomeipMessageParser::IsSetLampCommandRequest(
    const TransportMessage& transport_message) noexcept
{
    return IsRearLightingMessage(transport_message) &&
           (!transport_message.is_event) &&
           (transport_message.method_or_event_id ==
            domain::rear_lighting_service::kSetLampCommandMethodId);
}

bool SomeipMessageParser::IsGetLampStatusRequest(
    const TransportMessage& transport_message) noexcept
{
    return IsRearLightingMessage(transport_message) &&
           (!transport_message.is_event) &&
           (transport_message.method_or_event_id ==
            domain::rear_lighting_service::kGetLampStatusMethodId);
}

bool SomeipMessageParser::IsGetNodeHealthRequest(
    const TransportMessage& transport_message) noexcept
{
    return IsRearLightingMessage(transport_message) &&
           (!transport_message.is_event) &&
           (transport_message.method_or_event_id ==
            domain::rear_lighting_service::kGetNodeHealthMethodId);
}

bool SomeipMessageParser::IsLampStatusEvent(
    const TransportMessage& transport_message) noexcept
{
    return IsRearLightingMessage(transport_message) &&
           transport_message.is_event &&
           (transport_message.method_or_event_id ==
            domain::rear_lighting_service::kLampStatusEventId);
}

bool SomeipMessageParser::IsNodeHealthEvent(
    const TransportMessage& transport_message) noexcept
{
    return IsRearLightingMessage(transport_message) &&
           transport_message.is_event &&
           (transport_message.method_or_event_id ==
            domain::rear_lighting_service::kNodeHealthEventId);
}

domain::LampCommand SomeipMessageParser::ParseLampCommand(
    const TransportMessage& transport_message)
{
    domain::LampCommand lamp_command {};

    lamp_command.function = static_cast<domain::LampFunction>(
        ReadUint8(transport_message.payload, 0U));
    lamp_command.action = static_cast<domain::LampCommandAction>(
        ReadUint8(transport_message.payload, 1U));
    lamp_command.source = static_cast<domain::CommandSource>(
        ReadUint8(transport_message.payload, 2U));
    lamp_command.sequence_counter =
        ReadUint16(transport_message.payload, 3U);

    return lamp_command;
}

domain::LampFunction SomeipMessageParser::ParseLampFunction(
    const TransportMessage& transport_message)
{
    return static_cast<domain::LampFunction>(
        ReadUint8(transport_message.payload, 0U));
}

domain::LampStatus SomeipMessageParser::ParseLampStatus(
    const TransportMessage& transport_message)
{
    domain::LampStatus lamp_status {};

    lamp_status.function = static_cast<domain::LampFunction>(
        ReadUint8(transport_message.payload, 0U));
    lamp_status.output_state = static_cast<domain::LampOutputState>(
        ReadUint8(transport_message.payload, 1U));
    lamp_status.command_applied =
        (ReadUint8(transport_message.payload, 2U) != 0U);
    lamp_status.last_sequence_counter =
        ReadUint16(transport_message.payload, 3U);

    return lamp_status;
}

domain::NodeHealthStatus SomeipMessageParser::ParseNodeHealthStatus(
    const TransportMessage& transport_message)
{
    domain::NodeHealthStatus node_health_status {};

    node_health_status.health_state =
        static_cast<domain::NodeHealthState>(
            ReadUint8(transport_message.payload, 0U));
    node_health_status.ethernet_link_available =
        (ReadUint8(transport_message.payload, 1U) != 0U);
    node_health_status.service_available =
        (ReadUint8(transport_message.payload, 2U) != 0U);
    node_health_status.lamp_driver_fault_present =
        (ReadUint8(transport_message.payload, 3U) != 0U);
    node_health_status.active_fault_count =
        ReadUint16(transport_message.payload, 4U);

    return node_health_status;
}

}  // namespace body_control::lighting::transport
