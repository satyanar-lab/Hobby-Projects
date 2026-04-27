#include "body_control/lighting/transport/someip_message_builder.hpp"

#include <cstdint>

#include "body_control/lighting/domain/lighting_service_ids.hpp"

namespace body_control::lighting::transport
{
namespace
{

// Single-byte append; exists to keep all payload writes symmetric with AppendUint16.
void AppendUint8(
    std::vector<std::uint8_t>& payload,
    const std::uint8_t value)
{
    payload.push_back(value);
}

// Big-endian (SOME/IP wire order): high byte first, low byte second.
void AppendUint16(
    std::vector<std::uint8_t>& payload,
    const std::uint16_t value)
{
    payload.push_back(
        static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
    payload.push_back(
        static_cast<std::uint8_t>(value & 0xFFU));
}

// Stamps all fields that are the same for every rear-lighting message so each
// Build* function only needs to set the parts that differ (method/event ID, payload).
TransportMessage BuildBaseMessage(
    const std::uint16_t method_or_event_id,
    const std::uint16_t client_id,
    const std::uint16_t session_id,
    const bool is_event)
{
    TransportMessage transport_message {};

    transport_message.service_id =
        domain::rear_lighting_service::kServiceId;
    transport_message.instance_id =
        domain::rear_lighting_service::kInstanceId;
    transport_message.method_or_event_id = method_or_event_id;
    transport_message.client_id = client_id;
    transport_message.session_id = session_id;
    transport_message.is_event = is_event;
    transport_message.is_reliable = true;

    return transport_message;
}

}  // namespace

TransportMessage SomeipMessageBuilder::BuildSetLampCommandRequest(
    const domain::LampCommand& lamp_command,
    const std::uint16_t client_id,
    const std::uint16_t session_id)
{
    TransportMessage transport_message {
        BuildBaseMessage(
            domain::rear_lighting_service::kSetLampCommandMethodId,
            client_id,
            session_id,
            false)};

    AppendUint8(
        transport_message.payload,
        static_cast<std::uint8_t>(lamp_command.function));
    AppendUint8(
        transport_message.payload,
        static_cast<std::uint8_t>(lamp_command.action));
    AppendUint8(
        transport_message.payload,
        static_cast<std::uint8_t>(lamp_command.source));
    AppendUint16(
        transport_message.payload,
        lamp_command.sequence_counter);

    return transport_message;
}

TransportMessage SomeipMessageBuilder::BuildGetLampStatusRequest(
    const domain::LampFunction lamp_function,
    const std::uint16_t client_id,
    const std::uint16_t session_id)
{
    TransportMessage transport_message {
        BuildBaseMessage(
            domain::rear_lighting_service::kGetLampStatusMethodId,
            client_id,
            session_id,
            false)};

    AppendUint8(
        transport_message.payload,
        static_cast<std::uint8_t>(lamp_function));

    return transport_message;
}

TransportMessage SomeipMessageBuilder::BuildGetNodeHealthRequest(
    const std::uint16_t client_id,
    const std::uint16_t session_id)
{
    return BuildBaseMessage(
        domain::rear_lighting_service::kGetNodeHealthMethodId,
        client_id,
        session_id,
        false);
}

TransportMessage SomeipMessageBuilder::BuildLampStatusEvent(
    const domain::LampStatus& lamp_status)
{
    // Events carry no client or session context: client_id=0, session_id=0
    // per SOME/IP notification semantics. The is_event flag distinguishes
    // them from method responses at the receiver.
    TransportMessage transport_message {
        BuildBaseMessage(
            domain::rear_lighting_service::kLampStatusEventId,
            0U,
            0U,
            true)};

    AppendUint8(
        transport_message.payload,
        static_cast<std::uint8_t>(lamp_status.function));
    AppendUint8(
        transport_message.payload,
        static_cast<std::uint8_t>(lamp_status.output_state));
    AppendUint8(
        transport_message.payload,
        static_cast<std::uint8_t>(lamp_status.command_applied ? 1U : 0U));
    AppendUint16(
        transport_message.payload,
        lamp_status.last_sequence_counter);

    return transport_message;
}

TransportMessage SomeipMessageBuilder::BuildNodeHealthEvent(
    const domain::NodeHealthStatus& node_health_status)
{
    TransportMessage transport_message {
        BuildBaseMessage(
            domain::rear_lighting_service::kNodeHealthEventId,
            0U,
            0U,
            true)};

    AppendUint8(
        transport_message.payload,
        static_cast<std::uint8_t>(node_health_status.health_state));
    AppendUint8(
        transport_message.payload,
        static_cast<std::uint8_t>(
            node_health_status.ethernet_link_available ? 1U : 0U));
    AppendUint8(
        transport_message.payload,
        static_cast<std::uint8_t>(
            node_health_status.service_available ? 1U : 0U));
    AppendUint8(
        transport_message.payload,
        static_cast<std::uint8_t>(
            node_health_status.lamp_driver_fault_present ? 1U : 0U));
    AppendUint16(
        transport_message.payload,
        node_health_status.active_fault_count);

    return transport_message;
}

namespace
{

// Operator-service counterpart to BuildBaseMessage; same role but with the
// operator service_id/instance_id instead of the rear-lighting IDs.
TransportMessage BuildOperatorBaseMessage(
    const std::uint16_t method_or_event_id,
    const std::uint16_t client_id,
    const std::uint16_t session_id,
    const bool is_event)
{
    TransportMessage transport_message {};
    transport_message.service_id =
        domain::operator_service::kServiceId;
    transport_message.instance_id =
        domain::operator_service::kInstanceId;
    transport_message.method_or_event_id = method_or_event_id;
    transport_message.client_id = client_id;
    transport_message.session_id = session_id;
    transport_message.is_event = is_event;
    transport_message.is_reliable = true;
    return transport_message;
}

}  // namespace

TransportMessage SomeipMessageBuilder::BuildOperatorLampToggleRequest(
    const domain::LampFunction lamp_function,
    const std::uint16_t client_id,
    const std::uint16_t session_id)
{
    TransportMessage msg {BuildOperatorBaseMessage(
        domain::operator_service::kRequestLampToggleMethodId,
        client_id,
        session_id,
        false)};
    AppendUint8(msg.payload, static_cast<std::uint8_t>(lamp_function));
    return msg;
}

TransportMessage SomeipMessageBuilder::BuildOperatorLampActivateRequest(
    const domain::LampFunction lamp_function,
    const std::uint16_t client_id,
    const std::uint16_t session_id)
{
    TransportMessage msg {BuildOperatorBaseMessage(
        domain::operator_service::kRequestLampActivateMethodId,
        client_id,
        session_id,
        false)};
    AppendUint8(msg.payload, static_cast<std::uint8_t>(lamp_function));
    return msg;
}

TransportMessage SomeipMessageBuilder::BuildOperatorLampDeactivateRequest(
    const domain::LampFunction lamp_function,
    const std::uint16_t client_id,
    const std::uint16_t session_id)
{
    TransportMessage msg {BuildOperatorBaseMessage(
        domain::operator_service::kRequestLampDeactivateMethodId,
        client_id,
        session_id,
        false)};
    AppendUint8(msg.payload, static_cast<std::uint8_t>(lamp_function));
    return msg;
}

TransportMessage SomeipMessageBuilder::BuildOperatorNodeHealthRequest(
    const std::uint16_t client_id,
    const std::uint16_t session_id)
{
    return BuildOperatorBaseMessage(
        domain::operator_service::kRequestNodeHealthMethodId,
        client_id,
        session_id,
        false);
}

TransportMessage SomeipMessageBuilder::BuildOperatorLampStatusEvent(
    const domain::LampStatus& lamp_status)
{
    TransportMessage msg {BuildOperatorBaseMessage(
        domain::operator_service::kLampStatusEventId,
        0U,
        0U,
        true)};
    AppendUint8(msg.payload, static_cast<std::uint8_t>(lamp_status.function));
    AppendUint8(msg.payload, static_cast<std::uint8_t>(lamp_status.output_state));
    AppendUint8(
        msg.payload,
        static_cast<std::uint8_t>(lamp_status.command_applied ? 1U : 0U));
    AppendUint16(msg.payload, lamp_status.last_sequence_counter);
    return msg;
}

TransportMessage SomeipMessageBuilder::BuildOperatorNodeHealthEvent(
    const domain::NodeHealthStatus& node_health_status)
{
    TransportMessage msg {BuildOperatorBaseMessage(
        domain::operator_service::kNodeHealthEventId,
        0U,
        0U,
        true)};
    AppendUint8(
        msg.payload,
        static_cast<std::uint8_t>(node_health_status.health_state));
    AppendUint8(
        msg.payload,
        static_cast<std::uint8_t>(
            node_health_status.ethernet_link_available ? 1U : 0U));
    AppendUint8(
        msg.payload,
        static_cast<std::uint8_t>(
            node_health_status.service_available ? 1U : 0U));
    AppendUint8(
        msg.payload,
        static_cast<std::uint8_t>(
            node_health_status.lamp_driver_fault_present ? 1U : 0U));
    AppendUint16(msg.payload, node_health_status.active_fault_count);
    return msg;
}

}  // namespace body_control::lighting::transport
