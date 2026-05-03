#include "body_control/lighting/transport/someip_message_parser.hpp"

#include <cstddef>
#include <cstdint>

#include "body_control/lighting/domain/lighting_service_ids.hpp"

namespace body_control::lighting::transport
{
namespace
{

// Returns 0 on out-of-bounds rather than throwing; callers treat 0 as the
// kUnknown sentinel for all enum fields, so a truncated frame degrades to a
// safely-ignored unknown message rather than an exception or UB.
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

// Big-endian: high byte at index, low byte at index+1 (SOME/IP wire order).
// Returns 0 on out-of-bounds; same sentinel contract as ReadUint8.
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

// Pre-check shared by all rear-lighting predicates; avoids repeating the
// service_id/instance_id test in every Is* function.
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

namespace
{

// Same shared guard as IsRearLightingMessage, but for the operator service
// (HMI → CZC). Defined in a second anonymous namespace block so it is
// textually close to the operator-service predicates it serves.
[[nodiscard]] bool IsOperatorServiceMessage(
    const TransportMessage& transport_message) noexcept
{
    return (transport_message.service_id ==
            domain::operator_service::kServiceId) &&
           (transport_message.instance_id ==
            domain::operator_service::kInstanceId);
}

}  // namespace

bool SomeipMessageParser::IsOperatorLampToggleRequest(
    const TransportMessage& transport_message) noexcept
{
    return IsOperatorServiceMessage(transport_message) &&
           (!transport_message.is_event) &&
           (transport_message.method_or_event_id ==
            domain::operator_service::kRequestLampToggleMethodId);
}

bool SomeipMessageParser::IsOperatorLampActivateRequest(
    const TransportMessage& transport_message) noexcept
{
    return IsOperatorServiceMessage(transport_message) &&
           (!transport_message.is_event) &&
           (transport_message.method_or_event_id ==
            domain::operator_service::kRequestLampActivateMethodId);
}

bool SomeipMessageParser::IsOperatorLampDeactivateRequest(
    const TransportMessage& transport_message) noexcept
{
    return IsOperatorServiceMessage(transport_message) &&
           (!transport_message.is_event) &&
           (transport_message.method_or_event_id ==
            domain::operator_service::kRequestLampDeactivateMethodId);
}

bool SomeipMessageParser::IsOperatorNodeHealthRequest(
    const TransportMessage& transport_message) noexcept
{
    return IsOperatorServiceMessage(transport_message) &&
           (!transport_message.is_event) &&
           (transport_message.method_or_event_id ==
            domain::operator_service::kRequestNodeHealthMethodId);
}

bool SomeipMessageParser::IsOperatorLampStatusEvent(
    const TransportMessage& transport_message) noexcept
{
    return IsOperatorServiceMessage(transport_message) &&
           transport_message.is_event &&
           (transport_message.method_or_event_id ==
            domain::operator_service::kLampStatusEventId);
}

bool SomeipMessageParser::IsOperatorNodeHealthEvent(
    const TransportMessage& transport_message) noexcept
{
    return IsOperatorServiceMessage(transport_message) &&
           transport_message.is_event &&
           (transport_message.method_or_event_id ==
            domain::operator_service::kNodeHealthEventId);
}

bool SomeipMessageParser::IsInjectFaultRequest(
    const TransportMessage& transport_message) noexcept
{
    return IsRearLightingMessage(transport_message) &&
           (!transport_message.is_event) &&
           (transport_message.method_or_event_id ==
            domain::rear_lighting_service::kInjectLampFaultMethodId);
}

bool SomeipMessageParser::IsClearFaultRequest(
    const TransportMessage& transport_message) noexcept
{
    return IsRearLightingMessage(transport_message) &&
           (!transport_message.is_event) &&
           (transport_message.method_or_event_id ==
            domain::rear_lighting_service::kClearLampFaultMethodId);
}

bool SomeipMessageParser::IsGetFaultStatusRequest(
    const TransportMessage& transport_message) noexcept
{
    return IsRearLightingMessage(transport_message) &&
           (!transport_message.is_event) &&
           (transport_message.method_or_event_id ==
            domain::rear_lighting_service::kGetFaultStatusMethodId);
}

bool SomeipMessageParser::IsFaultStatusEvent(
    const TransportMessage& transport_message) noexcept
{
    return IsRearLightingMessage(transport_message) &&
           transport_message.is_event &&
           (transport_message.method_or_event_id ==
            domain::rear_lighting_service::kFaultStatusEventId);
}

domain::FaultCommand SomeipMessageParser::ParseFaultCommand(
    const TransportMessage& transport_message)
{
    domain::FaultCommand cmd {};
    cmd.function         = static_cast<domain::LampFunction>(
        ReadUint8(transport_message.payload, 0U));
    cmd.action           = static_cast<domain::FaultAction>(
        ReadUint8(transport_message.payload, 1U));
    cmd.source           = static_cast<domain::CommandSource>(
        ReadUint8(transport_message.payload, 2U));
    cmd.sequence_counter = ReadUint16(transport_message.payload, 3U);
    return cmd;
}

domain::LampFaultStatus SomeipMessageParser::ParseLampFaultStatus(
    const TransportMessage& transport_message)
{
    domain::LampFaultStatus status {};
    status.fault_present      = (ReadUint8(transport_message.payload, 0U) != 0U);
    status.active_fault_count = ReadUint8(transport_message.payload, 1U);

    for (std::size_t i = 0U; i < domain::LampFaultStatus::kMaxActiveDtcs; ++i)
    {
        status.active_faults[i] = static_cast<domain::FaultCode>(
            ReadUint16(transport_message.payload, 2U + (i * 2U)));
    }
    return status;
}

bool SomeipMessageParser::IsOperatorInjectFaultRequest(
    const TransportMessage& transport_message) noexcept
{
    return IsOperatorServiceMessage(transport_message) &&
           (!transport_message.is_event) &&
           (transport_message.method_or_event_id ==
            domain::operator_service::kRequestInjectFaultMethodId);
}

bool SomeipMessageParser::IsOperatorClearFaultRequest(
    const TransportMessage& transport_message) noexcept
{
    return IsOperatorServiceMessage(transport_message) &&
           (!transport_message.is_event) &&
           (transport_message.method_or_event_id ==
            domain::operator_service::kRequestClearFaultMethodId);
}

bool SomeipMessageParser::IsOperatorGetFaultStatusRequest(
    const TransportMessage& transport_message) noexcept
{
    return IsOperatorServiceMessage(transport_message) &&
           (!transport_message.is_event) &&
           (transport_message.method_or_event_id ==
            domain::operator_service::kRequestGetFaultStatusMethodId);
}

}  // namespace body_control::lighting::transport
