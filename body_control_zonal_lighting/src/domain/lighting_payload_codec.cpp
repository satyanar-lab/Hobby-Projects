#include "body_control/lighting/domain/lighting_payload_codec.hpp"

namespace body_control::lighting::domain
{
namespace
{

// Wire protocol uses 0x00/0x01 for boolean fields.  Any other value is a
// protocol violation caught by IsValidBooleanByte before decoding proceeds.
constexpr std::uint8_t kBooleanFalseValue {0U};
constexpr std::uint8_t kBooleanTrueValue {1U};

// SOME/IP specifies big-endian (network byte order) for all multi-byte fields.
void WriteUint16BigEndian(
    const std::uint16_t value,
    std::uint8_t& most_significant_byte,
    std::uint8_t& least_significant_byte) noexcept
{
    most_significant_byte  = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
    least_significant_byte = static_cast<std::uint8_t>(value & 0xFFU);
}

[[nodiscard]] std::uint16_t ReadUint16BigEndian(
    const std::uint8_t most_significant_byte,
    const std::uint8_t least_significant_byte) noexcept
{
    return static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(most_significant_byte) << 8U) |
        static_cast<std::uint16_t>(least_significant_byte));
}

// Rejects any byte that is not exactly 0x00 or 0x01.  A corrupted boolean
// byte (e.g. 0x02 from a bit-flip) would otherwise silently decode as false.
[[nodiscard]] bool IsValidBooleanByte(const std::uint8_t value) noexcept
{
    return (value == kBooleanFalseValue) || (value == kBooleanTrueValue);
}

[[nodiscard]] bool DecodeBooleanByte(const std::uint8_t value) noexcept
{
    return (value == kBooleanTrueValue);
}

}  // namespace

PayloadCodecStatus EncodeLampCommand(
    const LampCommand& command,
    LampCommandPayloadBuffer& payload_buffer) noexcept
{
    if (!IsValidLampCommand(command))
    {
        return PayloadCodecStatus::kInvalidArgument;
    }

    // Zero the whole buffer first so reserved bytes are always 0x00 on the wire,
    // giving deterministic output regardless of stack state.
    payload_buffer.fill(0U);

    payload_buffer[0] = static_cast<std::uint8_t>(command.function);
    payload_buffer[1] = static_cast<std::uint8_t>(command.action);
    payload_buffer[2] = static_cast<std::uint8_t>(command.source);
    WriteUint16BigEndian(
        command.sequence_counter,
        payload_buffer[4],
        payload_buffer[5]);

    return PayloadCodecStatus::kSuccess;
}

PayloadCodecStatus DecodeLampCommand(
    const std::uint8_t* payload_data,
    const std::size_t payload_length,
    LampCommand& command) noexcept
{
    if ((payload_data == nullptr) || (payload_length != kLampCommandPayloadLength))
    {
        return PayloadCodecStatus::kInvalidPayloadLength;
    }

    // Decode into a temporary first, validate, then assign.  This gives a
    // strong guarantee: command is never partially updated if validation fails.
    const LampCommand decoded_command {
        static_cast<LampFunction>(payload_data[0]),
        static_cast<LampCommandAction>(payload_data[1]),
        static_cast<CommandSource>(payload_data[2]),
        ReadUint16BigEndian(payload_data[4], payload_data[5])};

    if (!IsValidLampCommand(decoded_command))
    {
        return PayloadCodecStatus::kInvalidPayloadValue;
    }

    command = decoded_command;
    return PayloadCodecStatus::kSuccess;
}

PayloadCodecStatus EncodeLampStatus(
    const LampStatus& status,
    LampStatusPayloadBuffer& payload_buffer) noexcept
{
    if (!IsValidLampStatus(status))
    {
        return PayloadCodecStatus::kInvalidArgument;
    }

    payload_buffer.fill(0U);

    payload_buffer[0] = static_cast<std::uint8_t>(status.function);
    payload_buffer[1] = static_cast<std::uint8_t>(status.output_state);
    payload_buffer[2] = status.command_applied ? kBooleanTrueValue : kBooleanFalseValue;
    WriteUint16BigEndian(
        status.last_sequence_counter,
        payload_buffer[4],
        payload_buffer[5]);

    return PayloadCodecStatus::kSuccess;
}

PayloadCodecStatus DecodeLampStatus(
    const std::uint8_t* payload_data,
    const std::size_t payload_length,
    LampStatus& status) noexcept
{
    if ((payload_data == nullptr) || (payload_length != kLampStatusPayloadLength))
    {
        return PayloadCodecStatus::kInvalidPayloadLength;
    }

    // Validate boolean byte before constructing the struct to avoid
    // storing a status with an ambiguous command_applied value.
    if (!IsValidBooleanByte(payload_data[2]))
    {
        return PayloadCodecStatus::kInvalidPayloadValue;
    }

    const LampStatus decoded_status {
        static_cast<LampFunction>(payload_data[0]),
        static_cast<LampOutputState>(payload_data[1]),
        DecodeBooleanByte(payload_data[2]),
        ReadUint16BigEndian(payload_data[4], payload_data[5])};

    if (!IsValidLampStatus(decoded_status))
    {
        return PayloadCodecStatus::kInvalidPayloadValue;
    }

    status = decoded_status;
    return PayloadCodecStatus::kSuccess;
}

PayloadCodecStatus EncodeNodeHealthStatus(
    const NodeHealthStatus& status,
    NodeHealthStatusPayloadBuffer& payload_buffer) noexcept
{
    if (!IsValidNodeHealthStatus(status))
    {
        return PayloadCodecStatus::kInvalidArgument;
    }

    payload_buffer.fill(0U);

    payload_buffer[0] = static_cast<std::uint8_t>(status.health_state);
    payload_buffer[1] = status.ethernet_link_available  ? kBooleanTrueValue : kBooleanFalseValue;
    payload_buffer[2] = status.service_available        ? kBooleanTrueValue : kBooleanFalseValue;
    payload_buffer[3] = status.lamp_driver_fault_present ? kBooleanTrueValue : kBooleanFalseValue;
    WriteUint16BigEndian(
        status.active_fault_count,
        payload_buffer[4],
        payload_buffer[5]);

    return PayloadCodecStatus::kSuccess;
}

PayloadCodecStatus DecodeNodeHealthStatus(
    const std::uint8_t* payload_data,
    const std::size_t payload_length,
    NodeHealthStatus& status) noexcept
{
    if ((payload_data == nullptr) || (payload_length != kNodeHealthStatusPayloadLength))
    {
        return PayloadCodecStatus::kInvalidPayloadLength;
    }

    // Validate all three boolean bytes before constructing the struct.
    if ((!IsValidBooleanByte(payload_data[1])) ||
        (!IsValidBooleanByte(payload_data[2])) ||
        (!IsValidBooleanByte(payload_data[3])))
    {
        return PayloadCodecStatus::kInvalidPayloadValue;
    }

    const NodeHealthStatus decoded_status {
        static_cast<NodeHealthState>(payload_data[0]),
        DecodeBooleanByte(payload_data[1]),
        DecodeBooleanByte(payload_data[2]),
        DecodeBooleanByte(payload_data[3]),
        ReadUint16BigEndian(payload_data[4], payload_data[5])};

    if (!IsValidNodeHealthStatus(decoded_status))
    {
        return PayloadCodecStatus::kInvalidPayloadValue;
    }

    status = decoded_status;
    return PayloadCodecStatus::kSuccess;
}

PayloadCodecStatus EncodeFaultCommand(
    const FaultCommand& command,
    FaultCommandPayloadBuffer& payload_buffer) noexcept
{
    if (!IsValidFaultCommand(command))
    {
        return PayloadCodecStatus::kInvalidArgument;
    }

    payload_buffer.fill(0U);

    payload_buffer[0] = static_cast<std::uint8_t>(command.function);
    payload_buffer[1] = static_cast<std::uint8_t>(command.action);
    payload_buffer[2] = static_cast<std::uint8_t>(command.source);
    WriteUint16BigEndian(
        command.sequence_counter,
        payload_buffer[4],
        payload_buffer[5]);

    return PayloadCodecStatus::kSuccess;
}

PayloadCodecStatus DecodeFaultCommand(
    const std::uint8_t* payload_data,
    const std::size_t payload_length,
    FaultCommand& command) noexcept
{
    if ((payload_data == nullptr) || (payload_length != kFaultCommandPayloadLength))
    {
        return PayloadCodecStatus::kInvalidPayloadLength;
    }

    const FaultCommand decoded {
        static_cast<LampFunction>(payload_data[0]),
        static_cast<FaultAction>(payload_data[1]),
        static_cast<CommandSource>(payload_data[2]),
        ReadUint16BigEndian(payload_data[4], payload_data[5])};

    if (!IsValidFaultCommand(decoded))
    {
        return PayloadCodecStatus::kInvalidPayloadValue;
    }

    command = decoded;
    return PayloadCodecStatus::kSuccess;
}

PayloadCodecStatus EncodeLampFaultStatus(
    const LampFaultStatus& fault_status,
    LampFaultStatusPayloadBuffer& payload_buffer) noexcept
{
    payload_buffer.fill(0U);

    payload_buffer[0] = fault_status.fault_present ? kBooleanTrueValue : kBooleanFalseValue;
    payload_buffer[1] = fault_status.active_fault_count;

    for (std::size_t i = 0U; i < LampFaultStatus::kMaxActiveDtcs; ++i)
    {
        const std::uint16_t code = static_cast<std::uint16_t>(fault_status.active_faults[i]);
        WriteUint16BigEndian(
            code,
            payload_buffer[2U + (i * 2U)],
            payload_buffer[3U + (i * 2U)]);
    }

    return PayloadCodecStatus::kSuccess;
}

PayloadCodecStatus DecodeLampFaultStatus(
    const std::uint8_t* payload_data,
    const std::size_t payload_length,
    LampFaultStatus& fault_status) noexcept
{
    if ((payload_data == nullptr) || (payload_length != kLampFaultStatusPayloadLength))
    {
        return PayloadCodecStatus::kInvalidPayloadLength;
    }

    if (!IsValidBooleanByte(payload_data[0]))
    {
        return PayloadCodecStatus::kInvalidPayloadValue;
    }

    LampFaultStatus decoded {};
    decoded.fault_present      = DecodeBooleanByte(payload_data[0]);
    decoded.active_fault_count = payload_data[1];

    for (std::size_t i = 0U; i < LampFaultStatus::kMaxActiveDtcs; ++i)
    {
        decoded.active_faults[i] = static_cast<FaultCode>(
            ReadUint16BigEndian(
                payload_data[2U + (i * 2U)],
                payload_data[3U + (i * 2U)]));
    }

    fault_status = decoded;
    return PayloadCodecStatus::kSuccess;
}

}  // namespace body_control::lighting::domain
