#ifndef BODY_CONTROL_LIGHTING_DOMAIN_LIGHTING_PAYLOAD_CODEC_HPP_
#define BODY_CONTROL_LIGHTING_DOMAIN_LIGHTING_PAYLOAD_CODEC_HPP_

#include <array>
#include <cstddef>
#include <cstdint>

#include "body_control/lighting/domain/fault_types.hpp"
#include "body_control/lighting/domain/lamp_command_types.hpp"
#include "body_control/lighting/domain/lamp_status_types.hpp"

namespace body_control::lighting::domain
{

/**
 * Return status for all encode and decode operations in this codec.
 *
 * Every codec function is [[nodiscard]], so callers must inspect this before
 * using the output buffer or struct.
 */
enum class PayloadCodecStatus : std::uint8_t
{
    kSuccess              = 0U,  ///< Encode or decode completed without error.
    kInvalidArgument      = 1U,  ///< A domain struct field holds an out-of-range value.
    kInvalidPayloadLength = 2U,  ///< Incoming byte count does not match the expected fixed length.
    kInvalidPayloadValue  = 3U,  ///< A decoded byte maps to no known enum value.
};

/**
 * Fixed byte lengths for each message type carried over the network.
 *
 * All payloads are padded to 8 bytes with reserved bytes.  Fixed sizes remove
 * length-negotiation complexity and allow stack-allocated buffers throughout.
 */
constexpr std::size_t kLampCommandPayloadLength {8U};
constexpr std::size_t kLampStatusPayloadLength {8U};
constexpr std::size_t kNodeHealthStatusPayloadLength {8U};

/// Fault command (inject/clear) uses the same 8-byte layout rule.
constexpr std::size_t kFaultCommandPayloadLength {8U};

/// LampFaultStatus carries 5 × uint16 fault codes; 16 bytes total (12 used + 4 reserved).
constexpr std::size_t kLampFaultStatusPayloadLength {16U};

/// Stack-allocated buffer types — no heap allocation needed for any codec operation.
using LampCommandPayloadBuffer    = std::array<std::uint8_t, kLampCommandPayloadLength>;
using LampStatusPayloadBuffer     = std::array<std::uint8_t, kLampStatusPayloadLength>;
using NodeHealthStatusPayloadBuffer =
    std::array<std::uint8_t, kNodeHealthStatusPayloadLength>;
using FaultCommandPayloadBuffer    = std::array<std::uint8_t, kFaultCommandPayloadLength>;
using LampFaultStatusPayloadBuffer = std::array<std::uint8_t, kLampFaultStatusPayloadLength>;

/**
 * Serialise a LampCommand into a fixed 8-byte network payload (big-endian).
 *
 * Payload layout:
 *   Byte 0 : LampFunction        (uint8)
 *   Byte 1 : LampCommandAction   (uint8)
 *   Byte 2 : CommandSource       (uint8)
 *   Byte 3 : Reserved / padding
 *   Byte 4 : sequence_counter MSB
 *   Byte 5 : sequence_counter LSB
 *   Byte 6 : Reserved / padding
 *   Byte 7 : Reserved / padding
 *
 * @param command        Source command struct; must pass IsValidLampCommand().
 * @param payload_buffer Output buffer; overwritten on kSuccess, undefined otherwise.
 * @return kSuccess, or kInvalidArgument if any field is out of range.
 */
[[nodiscard]] PayloadCodecStatus EncodeLampCommand(
    const LampCommand& command,
    LampCommandPayloadBuffer& payload_buffer) noexcept;

/**
 * Deserialise a LampCommand from a raw byte buffer.
 *
 * @param payload_data   Pointer to the first byte of the received payload.
 * @param payload_length Must equal kLampCommandPayloadLength; returns
 *                       kInvalidPayloadLength otherwise.
 * @param command        Populated on kSuccess; left in initial state otherwise.
 * @return kSuccess, kInvalidPayloadLength, or kInvalidPayloadValue.
 */
[[nodiscard]] PayloadCodecStatus DecodeLampCommand(
    const std::uint8_t* payload_data,
    std::size_t payload_length,
    LampCommand& command) noexcept;

/**
 * Serialise a LampStatus into a fixed 8-byte network payload (big-endian).
 *
 * Payload layout:
 *   Byte 0 : LampFunction      (uint8)
 *   Byte 1 : LampOutputState   (uint8)
 *   Byte 2 : command_applied   (0x00 or 0x01)
 *   Byte 3 : Reserved / padding
 *   Byte 4 : last_sequence_counter MSB
 *   Byte 5 : last_sequence_counter LSB
 *   Byte 6 : Reserved / padding
 *   Byte 7 : Reserved / padding
 *
 * @param status         Source status struct to encode.
 * @param payload_buffer Output buffer; overwritten on kSuccess.
 * @return kSuccess or kInvalidArgument.
 */
[[nodiscard]] PayloadCodecStatus EncodeLampStatus(
    const LampStatus& status,
    LampStatusPayloadBuffer& payload_buffer) noexcept;

/**
 * Deserialise a LampStatus from a raw byte buffer.
 *
 * @param payload_data   Pointer to the first payload byte.
 * @param payload_length Must equal kLampStatusPayloadLength.
 * @param status         Populated on kSuccess.
 * @return kSuccess, kInvalidPayloadLength, or kInvalidPayloadValue.
 */
[[nodiscard]] PayloadCodecStatus DecodeLampStatus(
    const std::uint8_t* payload_data,
    std::size_t payload_length,
    LampStatus& status) noexcept;

/**
 * Serialise a NodeHealthStatus into a fixed 8-byte network payload (big-endian).
 *
 * Payload layout:
 *   Byte 0 : NodeHealthState              (uint8)
 *   Byte 1 : ethernet_link_available      (0x00 or 0x01)
 *   Byte 2 : service_available            (0x00 or 0x01)
 *   Byte 3 : lamp_driver_fault_present    (0x00 or 0x01)
 *   Byte 4 : active_fault_count MSB
 *   Byte 5 : active_fault_count LSB
 *   Byte 6 : Reserved / padding
 *   Byte 7 : Reserved / padding
 *
 * @param status         Source health snapshot to encode.
 * @param payload_buffer Output buffer; overwritten on kSuccess.
 * @return kSuccess or kInvalidArgument.
 */
[[nodiscard]] PayloadCodecStatus EncodeNodeHealthStatus(
    const NodeHealthStatus& status,
    NodeHealthStatusPayloadBuffer& payload_buffer) noexcept;

/**
 * Deserialise a NodeHealthStatus from a raw byte buffer.
 *
 * @param payload_data   Pointer to the first payload byte.
 * @param payload_length Must equal kNodeHealthStatusPayloadLength.
 * @param status         Populated on kSuccess.
 * @return kSuccess, kInvalidPayloadLength, or kInvalidPayloadValue.
 */
[[nodiscard]] PayloadCodecStatus DecodeNodeHealthStatus(
    const std::uint8_t* payload_data,
    std::size_t payload_length,
    NodeHealthStatus& status) noexcept;

/**
 * Serialise a FaultCommand into a fixed 8-byte network payload (big-endian).
 *
 * Payload layout:
 *   Byte 0 : LampFunction  (uint8)
 *   Byte 1 : FaultAction   (uint8)
 *   Byte 2 : CommandSource (uint8)
 *   Byte 3 : Reserved
 *   Byte 4 : sequence_counter MSB
 *   Byte 5 : sequence_counter LSB
 *   Byte 6 : Reserved
 *   Byte 7 : Reserved
 *
 * @return kSuccess, or kInvalidArgument if any field is out of range.
 */
[[nodiscard]] PayloadCodecStatus EncodeFaultCommand(
    const FaultCommand& command,
    FaultCommandPayloadBuffer& payload_buffer) noexcept;

/**
 * Deserialise a FaultCommand from a raw byte buffer.
 *
 * @return kSuccess, kInvalidPayloadLength, or kInvalidPayloadValue.
 */
[[nodiscard]] PayloadCodecStatus DecodeFaultCommand(
    const std::uint8_t* payload_data,
    std::size_t payload_length,
    FaultCommand& command) noexcept;

/**
 * Serialise a LampFaultStatus into a fixed 16-byte network payload (big-endian).
 *
 * Payload layout:
 *   Byte  0 : fault_present       (0x00 or 0x01)
 *   Byte  1 : active_fault_count  (uint8)
 *   Byte  2-3  : active_faults[0] (uint16, FaultCode)
 *   Byte  4-5  : active_faults[1]
 *   Byte  6-7  : active_faults[2]
 *   Byte  8-9  : active_faults[3]
 *   Byte 10-11 : active_faults[4]
 *   Bytes 12-15: Reserved
 *
 * @return kSuccess or kInvalidArgument.
 */
[[nodiscard]] PayloadCodecStatus EncodeLampFaultStatus(
    const LampFaultStatus& fault_status,
    LampFaultStatusPayloadBuffer& payload_buffer) noexcept;

/**
 * Deserialise a LampFaultStatus from a raw byte buffer.
 *
 * @return kSuccess, kInvalidPayloadLength, or kInvalidPayloadValue.
 */
[[nodiscard]] PayloadCodecStatus DecodeLampFaultStatus(
    const std::uint8_t* payload_data,
    std::size_t payload_length,
    LampFaultStatus& fault_status) noexcept;

}  // namespace body_control::lighting::domain

#endif  // BODY_CONTROL_LIGHTING_DOMAIN_LIGHTING_PAYLOAD_CODEC_HPP_
