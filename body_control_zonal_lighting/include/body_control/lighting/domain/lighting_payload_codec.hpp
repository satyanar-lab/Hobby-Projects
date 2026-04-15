#ifndef BODY_CONTROL_LIGHTING_DOMAIN_LIGHTING_PAYLOAD_CODEC_HPP_
#define BODY_CONTROL_LIGHTING_DOMAIN_LIGHTING_PAYLOAD_CODEC_HPP_

#include <array>
#include <cstddef>
#include <cstdint>

#include "body_control/lighting/domain/lamp_command_types.hpp"
#include "body_control/lighting/domain/lamp_status_types.hpp"

namespace body_control::lighting::domain
{

/**
 * @brief Result status for encode / decode operations.
 */
enum class PayloadCodecStatus : std::uint8_t
{
    kSuccess = 0U,
    kInvalidArgument = 1U,
    kInvalidPayloadLength = 2U,
    kInvalidPayloadValue = 3U
};

/**
 * @brief Fixed payload sizes used by the project.
 *
 * Fixed payload sizes keep the transport handling simple and deterministic.
 */
constexpr std::size_t kLampCommandPayloadLength {8U};
constexpr std::size_t kLampStatusPayloadLength {8U};
constexpr std::size_t kNodeHealthStatusPayloadLength {8U};

using LampCommandPayloadBuffer = std::array<std::uint8_t, kLampCommandPayloadLength>;
using LampStatusPayloadBuffer = std::array<std::uint8_t, kLampStatusPayloadLength>;
using NodeHealthStatusPayloadBuffer =
    std::array<std::uint8_t, kNodeHealthStatusPayloadLength>;

/**
 * @brief Encode a LampCommand into a fixed 8-byte payload.
 *
 * Payload layout:
 * Byte 0 : LampFunction
 * Byte 1 : LampCommandAction
 * Byte 2 : CommandSource
 * Byte 3 : Reserved
 * Byte 4 : Sequence counter MSB
 * Byte 5 : Sequence counter LSB
 * Byte 6 : Reserved
 * Byte 7 : Reserved
 */
[[nodiscard]] PayloadCodecStatus EncodeLampCommand(
    const LampCommand& command,
    LampCommandPayloadBuffer& payload_buffer) noexcept;

/**
 * @brief Decode a LampCommand from a raw payload buffer.
 */
[[nodiscard]] PayloadCodecStatus DecodeLampCommand(
    const std::uint8_t* payload_data,
    std::size_t payload_length,
    LampCommand& command) noexcept;

/**
 * @brief Encode a LampStatus into a fixed 8-byte payload.
 *
 * Payload layout:
 * Byte 0 : LampFunction
 * Byte 1 : LampOutputState
 * Byte 2 : Command applied flag (0 / 1)
 * Byte 3 : Reserved
 * Byte 4 : Last sequence counter MSB
 * Byte 5 : Last sequence counter LSB
 * Byte 6 : Reserved
 * Byte 7 : Reserved
 */
[[nodiscard]] PayloadCodecStatus EncodeLampStatus(
    const LampStatus& status,
    LampStatusPayloadBuffer& payload_buffer) noexcept;

/**
 * @brief Decode a LampStatus from a raw payload buffer.
 */
[[nodiscard]] PayloadCodecStatus DecodeLampStatus(
    const std::uint8_t* payload_data,
    std::size_t payload_length,
    LampStatus& status) noexcept;

/**
 * @brief Encode a NodeHealthStatus into a fixed 8-byte payload.
 *
 * Payload layout:
 * Byte 0 : NodeHealthState
 * Byte 1 : Ethernet link available flag (0 / 1)
 * Byte 2 : Service available flag (0 / 1)
 * Byte 3 : Lamp driver fault present flag (0 / 1)
 * Byte 4 : Active fault count MSB
 * Byte 5 : Active fault count LSB
 * Byte 6 : Reserved
 * Byte 7 : Reserved
 */
[[nodiscard]] PayloadCodecStatus EncodeNodeHealthStatus(
    const NodeHealthStatus& status,
    NodeHealthStatusPayloadBuffer& payload_buffer) noexcept;

/**
 * @brief Decode a NodeHealthStatus from a raw payload buffer.
 */
[[nodiscard]] PayloadCodecStatus DecodeNodeHealthStatus(
    const std::uint8_t* payload_data,
    std::size_t payload_length,
    NodeHealthStatus& status) noexcept;

}  // namespace body_control::lighting::domain

#endif  // BODY_CONTROL_LIGHTING_DOMAIN_LIGHTING_PAYLOAD_CODEC_HPP_