#ifndef BODY_CONTROL_LIGHTING_TRANSPORT_SOMEIP_MESSAGE_PARSER_HPP
#define BODY_CONTROL_LIGHTING_TRANSPORT_SOMEIP_MESSAGE_PARSER_HPP

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

/**
 * Stateless helper that classifies and parses inbound TransportMessages.
 *
 * Every service handler that receives a TransportMessage calls one of the
 * Is*() predicates to identify the message type, then calls the matching
 * Parse*() method to extract the domain object.  Separating classification
 * from parsing keeps the service handlers readable and the logic testable
 * in isolation.
 *
 * All methods are static — no instances are needed; the class is a
 * namespace-style grouping for related functions.
 *
 * Parse*() methods throw std::runtime_error if the payload cannot be decoded
 * (wrong length or unknown enum value); callers should guard with Is*() first.
 */
class SomeipMessageParser
{
public:
    // ── Rear lighting service predicates ────────────────────────────────────

    /** Returns true if the message is a SetLampCommand method call. */
    static bool IsSetLampCommandRequest(
        const TransportMessage& transport_message) noexcept;

    /** Returns true if the message is a GetLampStatus method call. */
    static bool IsGetLampStatusRequest(
        const TransportMessage& transport_message) noexcept;

    /** Returns true if the message is a GetNodeHealth method call. */
    static bool IsGetNodeHealthRequest(
        const TransportMessage& transport_message) noexcept;

    /** Returns true if the message is a LampStatus event push from the rear node. */
    static bool IsLampStatusEvent(
        const TransportMessage& transport_message) noexcept;

    /** Returns true if the message is a NodeHealthStatus event push from the rear node. */
    static bool IsNodeHealthEvent(
        const TransportMessage& transport_message) noexcept;

    // ── Rear lighting service parsers ────────────────────────────────────────

    /**
     * Decodes the payload of a SetLampCommand request into a LampCommand.
     *
     * @throws std::runtime_error if the payload length or values are invalid.
     */
    static domain::LampCommand ParseLampCommand(
        const TransportMessage& transport_message);

    /**
     * Extracts the LampFunction from a GetLampStatus request payload.
     *
     * @throws std::runtime_error if the payload cannot be decoded.
     */
    static domain::LampFunction ParseLampFunction(
        const TransportMessage& transport_message);

    /**
     * Decodes the payload of a LampStatus event or response into a LampStatus.
     *
     * @throws std::runtime_error if the payload length or values are invalid.
     */
    static domain::LampStatus ParseLampStatus(
        const TransportMessage& transport_message);

    /**
     * Decodes the payload of a NodeHealthStatus event or response.
     *
     * @throws std::runtime_error if the payload length or values are invalid.
     */
    static domain::NodeHealthStatus ParseNodeHealthStatus(
        const TransportMessage& transport_message);

    // ── Operator service predicates ──────────────────────────────────────────
    // These check the operator service service_id and the operator-specific
    // method IDs defined in lighting_service_ids.hpp::operator_service.

    /** Returns true if the message is an operator RequestLampToggle call. */
    [[nodiscard]] static bool IsOperatorLampToggleRequest(
        const TransportMessage& transport_message) noexcept;

    /** Returns true if the message is an operator RequestLampActivate call. */
    [[nodiscard]] static bool IsOperatorLampActivateRequest(
        const TransportMessage& transport_message) noexcept;

    /** Returns true if the message is an operator RequestLampDeactivate call. */
    [[nodiscard]] static bool IsOperatorLampDeactivateRequest(
        const TransportMessage& transport_message) noexcept;

    /** Returns true if the message is an operator RequestNodeHealth call. */
    [[nodiscard]] static bool IsOperatorNodeHealthRequest(
        const TransportMessage& transport_message) noexcept;

    /** Returns true if the message is a LampStatus event on the operator channel. */
    [[nodiscard]] static bool IsOperatorLampStatusEvent(
        const TransportMessage& transport_message) noexcept;

    /** Returns true if the message is a NodeHealthStatus event on the operator channel. */
    [[nodiscard]] static bool IsOperatorNodeHealthEvent(
        const TransportMessage& transport_message) noexcept;

    // Operator service parsers reuse ParseLampFunction / ParseLampStatus /
    // ParseNodeHealthStatus — the payload layout is identical to the rear
    // lighting service; only the service_id and method/event IDs differ.

    // ── Rear lighting service — fault predicates ─────────────────────────────

    /** Returns true if the message is an InjectFault method call on the rear lighting service. */
    [[nodiscard]] static bool IsInjectFaultRequest(
        const TransportMessage& transport_message) noexcept;

    /** Returns true if the message is a ClearFault method call on the rear lighting service. */
    [[nodiscard]] static bool IsClearFaultRequest(
        const TransportMessage& transport_message) noexcept;

    /** Returns true if the message is a GetFaultStatus method call on the rear lighting service. */
    [[nodiscard]] static bool IsGetFaultStatusRequest(
        const TransportMessage& transport_message) noexcept;

    /** Returns true if the message is a FaultStatus event from the rear node. */
    [[nodiscard]] static bool IsFaultStatusEvent(
        const TransportMessage& transport_message) noexcept;

    /**
     * Decodes the payload of an InjectFault or ClearFault request into a FaultCommand.
     *
     * @throws std::runtime_error if the payload is invalid.
     */
    static domain::FaultCommand ParseFaultCommand(
        const TransportMessage& transport_message);

    /**
     * Decodes the payload of a FaultStatus event into a LampFaultStatus.
     *
     * @throws std::runtime_error if the payload is invalid.
     */
    static domain::LampFaultStatus ParseLampFaultStatus(
        const TransportMessage& transport_message);

    // ── Operator service — fault predicates ──────────────────────────────────

    /** Returns true if the message is an operator InjectFault request. */
    [[nodiscard]] static bool IsOperatorInjectFaultRequest(
        const TransportMessage& transport_message) noexcept;

    /** Returns true if the message is an operator ClearFault request. */
    [[nodiscard]] static bool IsOperatorClearFaultRequest(
        const TransportMessage& transport_message) noexcept;

    /** Returns true if the message is an operator GetFaultStatus request. */
    [[nodiscard]] static bool IsOperatorGetFaultStatusRequest(
        const TransportMessage& transport_message) noexcept;
};

}  // namespace transport
}  // namespace lighting
}  // namespace body_control

#endif  // BODY_CONTROL_LIGHTING_TRANSPORT_SOMEIP_MESSAGE_PARSER_HPP
