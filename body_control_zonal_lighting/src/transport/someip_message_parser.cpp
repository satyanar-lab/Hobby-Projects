#include "body_control/lighting/transport/someip_message_parser.hpp"

#include <cstddef>
#include <cstdint>

#include "body_control/lighting/domain/lighting_service_ids.hpp"

namespace body_control
{
namespace lighting
{
namespace transport
{
namespace
{

std::uint8_t ReadUint8(
    const std::vector<std::uint8_t>& payload,
    const std::size_t index)
{
    if (index >= payload.size())
    {
        return 0U;
    }

    return payload[index];
}

std::uint32_t ReadUint32(
    const std::vector<std::uint8_t>& payload,
    const std::size_t index)
{
    if ((index + 3U) >= payload.size())
    {
        return 0U;
    }

    return (static_cast<std::uint32_t>(payload[index]) << 24U) |
           (static_cast<std::uint32_t>(payload[index + 1U]) << 16U) |
           (static_cast<std::uint32_t>(payload[index + 2U]) << 8U) |
           static_cast<std::uint32_t>(payload[index + 3U]);
}

bool IsRearLightingMessage(
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
        ReadUint32(transport_message.payload, 3U);

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
        ReadUint32(transport_message.payload, 3U);

    return lamp_status;
}

domain::NodeHealthStatus SomeipMessageParser::ParseNodeHealthStatus(
    const TransportMessage& transport_message)
{
    domain::NodeHealthStatus node_health_status {};

    node_health_status.node_state = static_cast<domain::NodeHealthState>(
        ReadUint8(transport_message.payload, 0U));
    node_health_status.ethernet_link_up =
        (ReadUint8(transport_message.payload, 1U) != 0U);
    node_health_status.service_available =
        (ReadUint8(transport_message.payload, 2U) != 0U);
    node_health_status.last_sequence_counter =
        ReadUint32(transport_message.payload, 3U);

    return node_health_status;
}

}  // namespace transport
}  // namespace lighting
}  // namespace body_control